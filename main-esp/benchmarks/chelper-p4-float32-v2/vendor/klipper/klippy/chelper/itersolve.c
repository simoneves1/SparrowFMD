// Iterative solver for kinematic moves
//
// Copyright (C) 2018-2020  Kevin O'Connor <kevin@koconnor.net>
//
// This file may be distributed under the terms of the GNU GPLv3 license.

#include <math.h> // fabs
#include <float.h> // FLT_EPSILON
#include <stddef.h> // offsetof
#include <string.h> // memset
#include "compiler.h" // __visible
#include "itersolve.h" // itersolve_generate_steps
#include "pyhelper.h" // errorf
#include "stepcompress.h" // queue_append_start
#include "trapq.h" // struct move


/****************************************************************
 * Main iterative solver
 ****************************************************************/

struct timepos {
    float time, position;
};

#define SEEK_TIME_RESET 0.000100

// Adaptive epsilons, replacing the original double-precision-calibrated
// fixed constants (1e-9/1e-8), which are unrepresentable in float32 at
// realistic magnitudes, and a first fixed-float32 attempt (1e-4), which
// was representable everywhere but too loose relative to half_step,
// causing ~80% step-count inflation (see RESULTS.md history). Each
// epsilon here is the larger of: a tolerance kept relative to the
// physically meaningful scale in play (half_step for distance; nothing
// analogous for time, see time_eps), and float32's own representable-ULP
// floor at the CURRENT magnitude of the value being compared (so the
// bracket search can still always converge and terminate, however large
// abs_start/print_time/position have grown over a real print) --
// standard relative/absolute-floor epsilon construction, not a single
// guessed constant.
#define ULP_SAFETY_FACTOR 8.0f

static inline float
dist_eps(float half_step, float magnitude)
{
    float rel = half_step * 0.01f;
    float ulp_floor = ULP_SAFETY_FACTOR * FLT_EPSILON * fabsf(magnitude);
    return rel > ulp_floor ? rel : ulp_floor;
}

static inline float
time_eps(float magnitude)
{
    float ulp_floor = ULP_SAFETY_FACTOR * FLT_EPSILON * fabsf(magnitude);
    float min_floor = 1e-7f; // matches the double original's intent at t~0
    return ulp_floor > min_floor ? ulp_floor : min_floor;
}

// Generate step times for a portion of a move
static int32_t
itersolve_gen_steps_range(struct stepper_kinematics *sk, struct stepcompress *sc
                          , struct move *m, float abs_start, float abs_end)
{
    sk_calc_callback calc_position_cb = sk->calc_position_cb;
    float half_step = .5 * sk->step_dist;
    float start = abs_start - m->print_time, end = abs_end - m->print_time;
    if (start < 0.)
        start = 0.;
    if (end > m->move_t)
        end = m->move_t;
    struct timepos old_guess = {start, sk->commanded_pos}, guess = old_guess;
    int sdir = stepcompress_get_step_dir(sc);
    int is_dir_change = 0, have_bracket = 0, check_oscillate = 0;
    float target = sk->commanded_pos + (sdir ? half_step : -half_step);
    float last_time=start, low_time=start, high_time=start + SEEK_TIME_RESET;
    if (high_time > end)
        high_time = end;
    for (;;) {
        // Use the "secant method" to guess a new time from previous guesses
        float guess_dist = guess.position - target;
        float og_dist = old_guess.position - target;
        float next_time = ((old_guess.time*guess_dist - guess.time*og_dist)
                            / (guess_dist - og_dist));
        if (!(next_time > low_time && next_time < high_time)) { // or NaN
            // Next guess is outside bounds checks - validate it
            if (have_bracket) {
                // A poor guess - fall back to bisection
                next_time = (low_time + high_time) * .5;
                check_oscillate = 0;
            } else if (guess.time >= end) {
                // No more steps present in requested time range
                break;
            } else {
                // Might be a poor guess - limit to exponential search
                next_time = high_time;
                high_time = 2. * high_time - last_time;
                if (high_time > end)
                    high_time = end;
            }
        }
        // Calculate position at next_time guess
        old_guess = guess;
        guess.time = next_time;
        guess.position = calc_position_cb(sk, m, next_time);
        guess_dist = guess.position - target;
        float d_eps = dist_eps(half_step, guess.position);
        if (fabsf(guess_dist) > d_eps) {
            // Guess does not look close enough - update bounds
            float rel_dist = sdir ? guess_dist : -guess_dist;
            if (rel_dist > 0.) {
                // Found position past target, so step is definitely present
                if (have_bracket && old_guess.time <= low_time) {
                    if (check_oscillate)
                        // Force bisect next to avoid persistent oscillations
                        old_guess = guess;
                    check_oscillate = 1;
                }
                high_time = guess.time;
                have_bracket = 1;
            } else if (rel_dist < -(half_step + half_step + d_eps)) {
                // Found direction change
                sdir = !sdir;
                target = (sdir ? target + half_step + half_step
                          : target - half_step - half_step);
                low_time = last_time;
                high_time = guess.time;
                is_dir_change = have_bracket = 1;
                check_oscillate = 0;
            } else {
                low_time = guess.time;
            }
            if (!have_bracket || high_time - low_time > time_eps(guess.time)) {
                if (!is_dir_change && rel_dist >= -half_step)
                    // Avoid rollback if stepper fully reaches step position
                    stepcompress_commit(sc);
                // Guess is not close enough - guess again with new time
                continue;
            }
        }
        // Found next step - submit it
        int ret = stepcompress_append(sc, sdir, m->print_time, guess.time);
        if (ret)
            return ret;
        target = sdir ? target+half_step+half_step : target-half_step-half_step;
        // Reset bounds checking
        float seek_time_delta = 1.5 * (guess.time - last_time);
        float t_eps = time_eps(guess.time);
        if (seek_time_delta < t_eps)
            seek_time_delta = t_eps;
        if (is_dir_change && seek_time_delta > SEEK_TIME_RESET)
            seek_time_delta = SEEK_TIME_RESET;
        last_time = low_time = guess.time;
        high_time = guess.time + seek_time_delta;
        if (high_time > end)
            high_time = end;
        is_dir_change = have_bracket = check_oscillate = 0;
    }
    sk->commanded_pos = target - (sdir ? half_step : -half_step);
    if (sk->post_cb)
        sk->post_cb(sk);
    return 0;
}


/****************************************************************
 * Interface functions
 ****************************************************************/

// Check if a move is likely to cause movement on a stepper
static inline int
check_active(struct stepper_kinematics *sk, struct move *m)
{
    int af = sk->active_flags;
    return ((af & AF_X && m->axes_r.x != 0.)
            || (af & AF_Y && m->axes_r.y != 0.)
            || (af & AF_Z && m->axes_r.z != 0.));
}

// Generate step times for a range of moves on the trapq
int32_t
itersolve_generate_steps(struct stepper_kinematics *sk, struct stepcompress *sc
                         , float flush_time)
{
    float last_flush_time = sk->last_flush_time;
    sk->last_flush_time = flush_time;
    if (!sk->tq)
        return 0;
    struct move *m = list_first_entry(&sk->tq->moves, struct move, node);
    while (last_flush_time >= m->print_time + m->move_t)
        m = list_next_entry(m, node);
    float force_steps_time = sk->last_move_time + sk->gen_steps_post_active;
    int skip_count = 0;
    for (;;) {
        float move_start = m->print_time, move_end = move_start + m->move_t;
        if (check_active(sk, m)) {
            if (skip_count && sk->gen_steps_pre_active) {
                // Must generate steps leading up to stepper activity
                float abs_start = move_start - sk->gen_steps_pre_active;
                if (abs_start < last_flush_time)
                    abs_start = last_flush_time;
                if (abs_start < force_steps_time)
                    abs_start = force_steps_time;
                struct move *pm = list_prev_entry(m, node);
                while (--skip_count && pm->print_time > abs_start)
                    pm = list_prev_entry(pm, node);
                do {
                    int32_t ret = itersolve_gen_steps_range(
                        sk, sc, pm, abs_start, flush_time);
                    if (ret)
                        return ret;
                    pm = list_next_entry(pm, node);
                } while (pm != m);
            }
            // Generate steps for this move
            int32_t ret = itersolve_gen_steps_range(sk, sc, m, last_flush_time
                                                    , flush_time);
            if (ret)
                return ret;
            if (move_end >= flush_time) {
                sk->last_move_time = flush_time;
                return 0;
            }
            skip_count = 0;
            sk->last_move_time = move_end;
            force_steps_time = sk->last_move_time + sk->gen_steps_post_active;
        } else {
            if (move_start < force_steps_time) {
                // Must generates steps just past stepper activity
                float abs_end = force_steps_time;
                if (abs_end > flush_time)
                    abs_end = flush_time;
                int32_t ret = itersolve_gen_steps_range(
                    sk, sc, m, last_flush_time, abs_end);
                if (ret)
                    return ret;
                skip_count = 1;
            } else {
                // This move doesn't impact this stepper - skip it
                skip_count++;
            }
            if (flush_time + sk->gen_steps_pre_active <= move_end)
                return 0;
        }
        m = list_next_entry(m, node);
    }
}

// Check if the given stepper is likely to be active in the given time range
float __visible
itersolve_check_active(struct stepper_kinematics *sk, float flush_time)
{
    if (!sk->tq)
        return 0.;
    trapq_check_sentinels(sk->tq);
    struct move *m = list_first_entry(&sk->tq->moves, struct move, node);
    while (sk->last_flush_time >= m->print_time + m->move_t)
        m = list_next_entry(m, node);
    for (;;) {
        if (check_active(sk, m))
            return m->print_time;
        if (flush_time <= m->print_time + m->move_t)
            return 0.;
        m = list_next_entry(m, node);
    }
}

// Report if the given stepper is registered for the given axis
int32_t __visible
itersolve_is_active_axis(struct stepper_kinematics *sk, char axis)
{
    if (axis < 'x' || axis > 'z')
        return 0;
    return (sk->active_flags & (AF_X << (axis - 'x'))) != 0;
}

void __visible
itersolve_set_trapq(struct stepper_kinematics *sk, struct trapq *tq
                    , float step_dist)
{
    sk->tq = tq;
    sk->step_dist = step_dist;
}

struct trapq * __visible
itersolve_get_trapq(struct stepper_kinematics *sk)
{
    return sk->tq;
}

float __visible
itersolve_calc_position_from_coord(struct stepper_kinematics *sk
                                   , float x, float y, float z)
{
    struct move m;
    memset(&m, 0, sizeof(m));
    m.start_pos.x = x;
    m.start_pos.y = y;
    m.start_pos.z = z;
    m.move_t = 1000.;
    return sk->calc_position_cb(sk, &m, 500.);
}

void __visible
itersolve_set_position(struct stepper_kinematics *sk
                       , float x, float y, float z)
{
    sk->commanded_pos = itersolve_calc_position_from_coord(sk, x, y, z);
}

float __visible
itersolve_get_commanded_pos(struct stepper_kinematics *sk)
{
    return sk->commanded_pos;
}

float __visible
itersolve_get_gen_steps_pre_active(struct stepper_kinematics *sk)
{
    return sk->gen_steps_pre_active;
}

float __visible
itersolve_get_gen_steps_post_active(struct stepper_kinematics *sk)
{
    return sk->gen_steps_post_active;
}
