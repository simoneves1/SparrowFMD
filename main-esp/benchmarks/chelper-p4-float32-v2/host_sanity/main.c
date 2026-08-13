// Host-native sanity/precision harness for the ADAPTIVE-epsilon float32
// chelper port (v2 of this prototype -- see RESULTS.md for what the first,
// fixed-epsilon attempt found and why it wasn't good enough).
//
// The first prototype established two separate things:
//   1. The iterative solver's hot loop rebases to MOVE-LOCAL time before
//      doing any epsilon-sensitive math (`start = abs_start - m->print_time`),
//      so absolute print_time magnitude does NOT affect convergence or
//      relative step-to-step spacing -- confirmed by that prototype's
//      identical relative-spacing results across print_time offsets.
//   2. What DOES degrade with magnitude is storing print_time itself in a
//      float32 struct field (unavoidable, not an epsilon question) and,
//      separately, the DISTANCE epsilon's soundness depends on POSITION
//      magnitude (coordinates), which is bed-scale-bounded (tens to a few
//      hundred mm), not unboundedly growing like print_time is.
//
// So this harness sweeps START POSITION magnitude (simulating how far
// across the bed a move is, not how far into the print in time) to test
// whether the adaptive epsilon (itersolve.c's dist_eps()/time_eps(),
// scaled to half_step and to float32's own ULP floor at the magnitude in
// play) produces a clean, correct step count at realistic bed-scale
// coordinates -- the thing the first prototype's fixed epsilon failed at
// (either non-convergent at 1e-9, or ~80% step-count inflation at 1e-4).
// print_time offset is also swept (fixed per-position-case) to keep
// reporting the known, separate, unfixable-by-epsilon absolute-timestamp
// storage error for completeness.

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "trapq.h"
#include "itersolve.h"
#include "stub_stepcompress.h"

struct stepper_kinematics *cartesian_stepper_alloc(char axis);
struct stepcompress *stepcompress_alloc(struct list_head *msg_queue);

#define STEP_DIST 0.005f   // mm/step -- 200 steps/mm

struct move_profile {
    float accel_t, cruise_t, decel_t;
    float start_v, cruise_v, accel;
    float distance;
};

static struct move_profile
make_profile(void)
{
    struct move_profile p;
    p.accel = 1000.f;
    p.accel_t = 0.1f;
    p.cruise_v = p.accel * p.accel_t;   // 100 mm/s
    p.cruise_t = 0.5f;
    p.decel_t = 0.1f;
    p.start_v = 0.f;
    float accel_dist = 0.5f * p.accel * p.accel_t * p.accel_t;
    float cruise_dist = p.cruise_v * p.cruise_t;
    p.distance = accel_dist + cruise_dist + accel_dist;
    return p;
}

// Runs the move profile starting at print_time=t0_true and bed position
// x=pos0, and reports step-count/spacing correctness plus the known
// absolute-timestamp storage error.
static void
run_case(const char *label, double t0_true, float pos0)
{
    struct move_profile p = make_profile();
    float t0 = (float)t0_true;

    struct trapq *tq = trapq_alloc();
    trapq_append(tq, t0, p.accel_t, p.cruise_t, p.decel_t
                , pos0, 0.f, 0.f
                , 1.f, 0.f, 0.f
                , p.start_v, p.cruise_v, p.accel);

    struct stepper_kinematics *sk = cartesian_stepper_alloc('x');
    itersolve_set_trapq(sk, tq, STEP_DIST);
    itersolve_set_position(sk, pos0, 0.f, 0.f);

    struct stepcompress *sc = stepcompress_alloc(NULL);
    sk->sc = sc;

    float move_end = t0 + p.accel_t + p.cruise_t + p.decel_t;
    int32_t ret = itersolve_generate_steps(sk, sc, move_end + 1.f);
    stepcompress_finalize(sc);

    long steps = stepcompress_get_step_count(sc);
    long nonmono = stepcompress_get_nonmonotonic_count(sc);
    long zero = stepcompress_get_zero_delta_count(sc);
    double min_delta = stepcompress_get_min_positive_delta(sc);
    double max_delta = stepcompress_get_max_delta(sc);
    double expected_steps = p.distance / STEP_DIST;
    double expected_min_delta = STEP_DIST / p.cruise_v;
    double pct_off = 100.0 * ((double)steps - expected_steps) / expected_steps;

    double t0_storage_err = (double)t0 - t0_true;

    printf("== %-38s pos0=%7.1fmm t0=%12.3fs ==\n", label, (double)pos0, t0_true);
    printf("   steps: %6ld  expected: %6.1f  (%+.2f%%)   nonmono: %ld  zero_delta: %ld\n",
           steps, expected_steps, pct_off, nonmono, zero);
    printf("   min_delta: %.9fs (expect %.9fs, err %.3e)   max_delta: %.9fs\n",
           min_delta, expected_min_delta, min_delta - expected_min_delta, max_delta);
    printf("   print_time storage error: %.3e s%s\n\n",
           t0_storage_err, fabs(t0_storage_err) > 1e-9 ? "  (unfixable by epsilon -- storage, not convergence)" : "");

    free(sc);
    free(sk);
    trapq_free(tq);
}

int
main(void)
{
    printf("chelper float32 port v2 (adaptive epsilon) -- sweep report\n");
    printf("STEP_DIST = %.4f mm/step (200 steps/mm)\n\n", STEP_DIST);

    printf("--- position-magnitude sweep (t0 fixed at 3617.234567s, ~1hr in) ---\n");
    run_case("origin corner", 3617.234567, 0.f);
    run_case("small printer edge (~120mm)", 3617.234567, 120.f);
    run_case("typical bed edge (~250mm)", 3617.234567, 250.f);
    run_case("large-format bed edge (~500mm)", 3617.234567, 500.f);
    run_case("very large gantry (~1000mm)", 3617.234567, 1000.f);

    printf("--- print_time-magnitude sweep (pos0 fixed at 250mm) ---\n");
    run_case("start of print", 17.234567, 250.f);
    run_case("+1 hour", 3617.234567, 250.f);
    run_case("+8 hours", 28817.234567, 250.f);
    run_case("+24 hours (very long print)", 86417.234567, 250.f);
    run_case("+72 hours (multi-day print)", 259217.234567, 250.f);

    return 0;
}
