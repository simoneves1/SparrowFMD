// Kinematics benchmark harness (ESP-IDF / esp32p4 target): exercises
// Klipper's chelper trapq + itersolve + stepcompress path -- the code
// that would need to run on main-esp's P4 -- against a
// synthetic-but-representative FDM move sequence, and times how long the
// CPU-heavy step generation takes on real (or QEMU-emulated) hardware.
//
// This mirrors ../host_sanity/harness_main.c exactly in logic; only the
// timing source, entry point, and (optionally, via -DNUM_SEGMENTS=N)
// segment count differ. Keep the two in sync if the move-generation
// logic changes.
//
// This is a spike, not production code: no real sliced G-code file was
// available, so the move sequence below is generated from typical FDM
// print parameters (zigzag infill + periodic layer lifts) rather than
// pulled from an actual print. See ../RESULTS.md for that caveat.
//
// NUM_SEGMENTS is overridable at build time: the default (4000) is sized
// for the P4's much larger RAM, but was too large to fit in an
// esp32c3's ~330KB heap for this un-pruned single-flush-at-the-end
// harness -- see RESULTS.md, "memory budget" section, for why and what
// a real (non-benchmark) implementation would do differently.
#include <math.h>
#include <stdio.h>
#include <stdint.h>
#include "esp_timer.h"
#include "esp_log.h"
#include "trapq.h"
#include "itersolve.h"
#include "stepcompress.h"

static const char *TAG = "bench";

struct stepper_kinematics *cartesian_stepper_alloc(char axis);

static double
now_seconds(void)
{
    return (double)esp_timer_get_time() * 1e-6;
}

#define ACCEL 2500.0          // mm/s^2, typical Cartesian FDM default
#define XY_STEP_DIST 0.0125   // mm/step: 2mm GT2, 20T pulley, 16 usteps
#define Z_STEP_DIST 0.0025    // mm/step: 8mm/rev TR8 leadscrew, 16 usteps
#define MCU_FREQ 72000000.0   // Hz, typical Klipper MCU timer clock
#ifndef NUM_SEGMENTS
#define NUM_SEGMENTS 4000     // "short representative excerpt"
#endif

struct axis_setup {
    struct stepper_kinematics *sk;
    struct stepcompress *sc;
    struct list_head msg_queue;
};

static void
setup_axis(struct axis_setup *a, char axis, struct trapq *tq, double step_dist)
{
    a->sk = cartesian_stepper_alloc(axis);
    itersolve_set_trapq(a->sk, tq, step_dist);
    list_init(&a->msg_queue);
    a->sc = stepcompress_alloc(&a->msg_queue);
    stepcompress_fill(a->sc, 0, (uint32_t)(0.000025 * MCU_FREQ), 0, 0);
    stepcompress_set_time(a->sc, 0.0, MCU_FREQ);
}

static void
plan_move(double distance, double cruise_v, double accel
         , double *accel_t, double *cruise_t, double *decel_t
         , double *actual_cruise_v)
{
    double accel_dist = (cruise_v * cruise_v) / accel;
    if (accel_dist <= distance) {
        double cruise_dist = distance - accel_dist;
        *accel_t = cruise_v / accel;
        *cruise_t = cruise_dist / cruise_v;
        *decel_t = *accel_t;
        *actual_cruise_v = cruise_v;
    } else {
        double peak_v = sqrt(accel * distance);
        *accel_t = peak_v / accel;
        *cruise_t = 0.0;
        *decel_t = *accel_t;
        *actual_cruise_v = peak_v;
    }
}

void
app_main(void)
{
    struct trapq *tq = trapq_alloc();

    struct axis_setup ax, ay, az;
    setup_axis(&ax, 'x', tq, XY_STEP_DIST);
    setup_axis(&ay, 'y', tq, XY_STEP_DIST);
    setup_axis(&az, 'z', tq, Z_STEP_DIST);

    double print_time = 0.0;
    double pos_x = 0.0, pos_y = 0.0, pos_z = 0.0;
    double total_distance = 0.0;
    unsigned seed = 12345;

    double gen_start = now_seconds();
    for (int i = 0; i < NUM_SEGMENTS; i++) {
        seed = seed * 1103515245u + 12345u;
        double r1 = (double)((seed >> 16) & 0x7fff) / 32768.0;
        seed = seed * 1103515245u + 12345u;
        double r2 = (double)((seed >> 16) & 0x7fff) / 32768.0;

        double axr_x = 0, axr_y = 0, axr_z = 0;
        double distance;
        double cruise_v;

        if (i % 250 == 249) {
            axr_z = 1.0;
            distance = 0.2;
            cruise_v = 10.0;
        } else if (i % 2 == 0) {
            axr_x = (i % 4 == 0) ? 1.0 : -1.0;
            distance = 0.4 + r1 * 19.6;
            cruise_v = 20.0 + r2 * 100.0;
        } else {
            axr_y = 1.0;
            distance = 0.4;
            cruise_v = 20.0;
        }

        double accel_t, cruise_t, decel_t, actual_cruise_v;
        plan_move(distance, cruise_v, ACCEL, &accel_t, &cruise_t, &decel_t
                 , &actual_cruise_v);

        trapq_append(tq, print_time, accel_t, cruise_t, decel_t
                    , pos_x, pos_y, pos_z, axr_x, axr_y, axr_z
                    , 0.0, actual_cruise_v, ACCEL);

        print_time += accel_t + cruise_t + decel_t;
        pos_x += axr_x * distance;
        pos_y += axr_y * distance;
        pos_z += axr_z * distance;
        total_distance += distance;
    }
    double gen_elapsed = now_seconds() - gen_start;

    double step_start = now_seconds();
    int32_t rx = itersolve_generate_steps(ax.sk, ax.sc, print_time);
    int32_t ry = itersolve_generate_steps(ay.sk, ay.sc, print_time);
    int32_t rz = itersolve_generate_steps(az.sk, az.sc, print_time);
    double step_elapsed = now_seconds() - step_start;

    if (rx || ry || rz) {
        ESP_LOGE(TAG, "itersolve_generate_steps error: x=%ld y=%ld z=%ld"
                , (long)rx, (long)ry, (long)rz);
        return;
    }

    double approx_steps = total_distance / XY_STEP_DIST;
    double moves_per_sec = gen_elapsed > 0
        ? NUM_SEGMENTS / gen_elapsed : (double)NUM_SEGMENTS / 1e-9;
    double step_gen_moves_per_sec = step_elapsed > 0
        ? NUM_SEGMENTS / step_elapsed : (double)NUM_SEGMENTS / 1e-9;

    printf("=== chelper kinematics benchmark (target build) ===\n");
    printf("segments (G-code-equivalent moves): %d\n", NUM_SEGMENTS);
    printf("simulated print time:               %.3f s\n", print_time);
    printf("approx total step count (X/Y axes): %.0f\n", approx_steps);
    printf("\n");
    printf("trapq_append phase:      %.6f s total, %.0f moves/sec\n"
          , gen_elapsed, moves_per_sec);
    printf("itersolve_generate_steps phase (x+y+z): %.6f s total, "
          "%.0f moves/sec-equivalent\n", step_elapsed, step_gen_moves_per_sec);
}
