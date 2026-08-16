// Host-buildable unit tests for the kinematics module (Cartesian only).
// Build with any C compiler, no ESP-IDF required (run from this file's
// own directory):
//   gcc -Wall -I../include -I../src -o kinematics_test test_main.c
//     ../src/trapq.c ../src/itersolve.c ../src/kin_cartesian.c
//     ../src/kinematics_steps.c -lm
//
// Exercises the same accel/cruise/decel move profile and magnitude
// sweeps (bed-scale position offsets, print-duration-scale time offsets)
// that validated the adaptive-epsilon float32 port during the original
// prototype -- see README.md and itersolve.c's comments for why those
// sweeps matter (a fixed epsilon either doesn't converge or produces
// duplicate-step artifacts; relative step spacing needs to stay
// magnitude-independent even though absolute print_time storage doesn't).
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "trapq.h"
#include "itersolve.h"
#include "kin_cartesian.h"
#include "kinematics_steps.h"

static int g_failures = 0;

#define CHECK(desc, cond) do { \
    if (cond) { \
        printf("PASS: %s\n", desc); \
    } else { \
        printf("FAIL: %s\n", desc); \
        g_failures++; \
    } \
} while (0)

#define STEP_DIST 0.005f // mm/step -- 200 steps/mm

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
    p.cruise_v = p.accel * p.accel_t; // 100 mm/s
    p.cruise_t = 0.5f;
    p.decel_t = 0.1f;
    p.start_v = 0.f;
    float accel_dist = 0.5f * p.accel * p.accel_t * p.accel_t;
    float cruise_dist = p.cruise_v * p.cruise_t;
    p.distance = accel_dist + cruise_dist + accel_dist;
    return p;
}

struct step_stats {
    long count;
    int has_prev;
    double prev_time;
    long nonmonotonic;
    double min_positive_delta;
    double max_delta;
};

static void
on_step(int dir, double time, void *ctx)
{
    (void)dir;
    struct step_stats *s = ctx;
    if (s->has_prev) {
        double delta = time - s->prev_time;
        if (delta <= 0.)
            s->nonmonotonic++;
        if (delta > 0. && (s->min_positive_delta < 0. || delta < s->min_positive_delta))
            s->min_positive_delta = delta;
        if (delta > s->max_delta)
            s->max_delta = delta;
    } else {
        s->has_prev = 1;
    }
    s->prev_time = time;
    s->count++;
}

// Runs the accel/cruise/decel X move profile starting at print_time=t0
// and bed position x=pos0, returning step statistics gathered via the
// kinematics_steps callback.
static struct step_stats
run_case(double t0_true, float pos0)
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

    struct step_stats stats = {0};
    stats.min_positive_delta = -1.;
    struct stepcompress *sc = kinematics_steps_alloc(on_step, &stats);
    sk->sc = sc;

    float move_end = t0 + p.accel_t + p.cruise_t + p.decel_t;
    itersolve_generate_steps(sk, sc, move_end + 1.f);
    kinematics_steps_finalize(sc);

    kinematics_steps_free(sc);
    free(sk);
    trapq_free(tq);
    return stats;
}

static void
test_basic_move_step_count_and_spacing(void)
{
    struct move_profile p = make_profile();
    struct step_stats s = run_case(17.234567, 250.f);

    double expected_steps = p.distance / STEP_DIST;
    double pct_off = 100.0 * ((double)s.count - expected_steps) / expected_steps;
    CHECK("step count is within 1% of the analytically expected count"
         , fabs(pct_off) < 1.0);

    double expected_min_delta = STEP_DIST / p.cruise_v;
    CHECK("min step delta at cruise speed matches expectation closely"
         , fabs(s.min_positive_delta - expected_min_delta) < 1e-5);

    CHECK("no more than a couple of non-monotonic steps out of thousands"
         , s.nonmonotonic <= 2);
}

static void
test_position_magnitude_sweep(void)
{
    struct move_profile p = make_profile();
    double expected_steps = p.distance / STEP_DIST;
    float positions[] = {0.f, 120.f, 250.f, 500.f};

    for (size_t i = 0; i < sizeof(positions) / sizeof(positions[0]); i++) {
        struct step_stats s = run_case(3617.234567, positions[i]);
        double pct_off = 100.0 * ((double)s.count - expected_steps) / expected_steps;
        char desc[128];
        snprintf(desc, sizeof(desc)
                , "step count within 1%% of expected at pos0=%.0fmm (got %+.2f%%)"
                , (double)positions[i], pct_off);
        CHECK(desc, fabs(pct_off) < 1.0);
    }
}

static void
test_relative_spacing_independent_of_print_time(void)
{
    // Klipper's own itersolve_gen_steps_range rebases to move-local time
    // before doing any precision-sensitive math, so relative step
    // spacing should be identical (to float32 precision) regardless of
    // how large absolute print_time has grown -- only the absolute
    // timestamp's storage precision degrades with magnitude, not the
    // spacing between steps. This is what makes the float32 port viable
    // at all for real, hours-long prints.
    double offsets[] = {17.234567, 3617.234567, 28817.234567, 259217.234567};
    double first_min_delta = -1.;
    int all_match = 1;

    for (size_t i = 0; i < sizeof(offsets) / sizeof(offsets[0]); i++) {
        struct step_stats s = run_case(offsets[i], 250.f);
        if (first_min_delta < 0.) {
            first_min_delta = s.min_positive_delta;
        } else if (fabs(s.min_positive_delta - first_min_delta) > 1e-9) {
            all_match = 0;
        }
    }
    CHECK("relative step spacing is print_time-offset-independent"
          " (start of print through 72 hours in)"
         , all_match);
}

int
main(void)
{
    test_basic_move_step_count_and_spacing();
    test_position_magnitude_sweep();
    test_relative_spacing_independent_of_print_time();

    printf("\n%s (%d failure%s)\n"
          , g_failures ? "SOME TESTS FAILED" : "ALL TESTS PASSED"
          , g_failures, g_failures == 1 ? "" : "s");
    return g_failures ? 1 : 0;
}
