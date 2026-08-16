// Host-buildable unit tests for motion-planner. Build with any C
// compiler, no ESP-IDF required (run from this file's own directory):
//   gcc -Wall -I../include -I../../gcode-parser/include
//     -I../../kinematics/include -I../../kinematics/src
//     -o motion_planner_test test_main.c ../src/motion_planner.c
//     ../../gcode-parser/src/gcode_parser.c
//     ../../kinematics/src/trapq.c ../../kinematics/src/itersolve.c
//     ../../kinematics/src/kin_cartesian.c
//     ../../kinematics/src/kinematics_steps.c -lm
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "motion_planner.h"
#include "gcode_parser.h"

static int g_failures = 0;

#define CHECK(desc, cond) do { \
    if (cond) { \
        printf("PASS: %s\n", desc); \
    } else { \
        printf("FAIL: %s\n", desc); \
        g_failures++; \
    } \
} while (0)

struct step_counts {
    long x, y, z;
    double last_time;
    int nonmonotonic;
};

static void
count_steps(const struct motion_planner_step_event *ev, void *ctx)
{
    struct step_counts *c = ctx;
    if (ev->time < c->last_time)
        c->nonmonotonic++;
    c->last_time = ev->time;
    if (ev->axis == 'x') c->x++;
    else if (ev->axis == 'y') c->y++;
    else if (ev->axis == 'z') c->z++;
}

static struct motion_planner *
make_planner(motion_planner_step_cb cb, void *ctx)
{
    struct motion_planner_config cfg = {
        .step_dist_x = 0.005f, .step_dist_y = 0.005f, .step_dist_z = 0.005f
        , .max_velocity_mm_s = 150.f, .max_accel_mm_s2 = 1500.f
        , .on_step = cb, .cb_ctx = ctx,
    };
    return motion_planner_create(&cfg);
}

static void
feed(struct motion_planner *mp, const char *line)
{
    struct gcode_command cmd;
    enum gcode_status st = gcode_parse_line(line, strlen(line), &cmd);
    if (st != GCODE_OK) {
        printf("FAIL: gcode_parse_line rejected a line the test expected"
               " to parse: \"%s\" (status %d)\n", line, st);
        g_failures++;
        return;
    }
    if (!motion_planner_handle_gcode(mp, &cmd)) {
        printf("FAIL: motion_planner_handle_gcode rejected: \"%s\"\n", line);
        g_failures++;
    }
}

static void
test_single_move_x_axis(void)
{
    struct step_counts c = {0};
    struct motion_planner *mp = make_planner(count_steps, &c);
    feed(mp, "G1 X60 F6000"); // 100mm/s, well under max_velocity

    double expected_steps = 60.0 / 0.005;
    double pct_off = 100.0 * ((double)c.x - expected_steps) / expected_steps;
    CHECK("a pure X move produces only X steps (no Y/Z)"
         , c.y == 0 && c.z == 0);
    CHECK("X step count is within 1% of the analytically expected count"
         , fabs(pct_off) < 1.0);
    CHECK("no non-monotonic steps", c.nonmonotonic == 0);

    motion_planner_destroy(mp);
}

static void
test_multiple_short_moves_none_dropped(void)
{
    // Each move here is well under 1 second -- this is exactly the
    // scenario that exposed the "flush_time = time_cursor + 1s margin"
    // bug during development: with that margin, the second (and any
    // subsequent) short move's steps were silently never generated,
    // because itersolve's last_flush_time bookkeeping thought it had
    // already covered past their end. See motion_planner.c's comment on
    // queue_move()'s flush_time calculation.
    struct step_counts c = {0};
    struct motion_planner *mp = make_planner(count_steps, &c);

    feed(mp, "G1 X5 F6000");  // ~0.05s move
    long after_first = c.x;
    feed(mp, "G1 X10 F6000"); // another short move
    feed(mp, "G1 X15 F6000");
    feed(mp, "G1 X20 F6000");

    CHECK("the first short move produced a plausible step count on its own"
         , after_first > 500); // 5mm/0.005 = 1000 steps, allow slack for accel/decel rounding
    CHECK("later short moves also produced steps (none silently dropped)"
         , c.x > after_first * 3); // 4 similar-sized moves should be ~4x one move's count
    CHECK("still no non-monotonic steps across multiple queued moves"
         , c.nonmonotonic == 0);

    motion_planner_destroy(mp);
}

static void
test_modal_axis_position_preserved(void)
{
    // Second line omits X -- it must stay at the position the first line
    // left it at (10), not reset to 0. If that regressed, this Y-only
    // move would incorrectly also show X steps (dx = 0 - 10 = -10).
    struct step_counts c = {0};
    struct motion_planner *mp = make_planner(count_steps, &c);

    feed(mp, "G1 X10 F3000");
    long x_after_first = c.x;
    feed(mp, "G1 Y10"); // no X -- should stay at 10, no further X movement

    CHECK("a Y-only move (X omitted) produces no additional X steps"
         , c.x == x_after_first);
    CHECK("the Y-only move did produce Y steps", c.y > 0);

    motion_planner_destroy(mp);
}

static void
test_non_move_commands_ignored(void)
{
    struct step_counts c = {0};
    struct motion_planner *mp = make_planner(count_steps, &c);

    feed(mp, "M104 S200");  // set hotend temp -- not a move, no G28/G92 handling either

    CHECK("a non-G command produces no steps and is not treated as an error"
         , c.x == 0 && c.y == 0 && c.z == 0);

    motion_planner_destroy(mp);
}

static void
test_g28_and_g92_produce_no_steps_themselves(void)
{
    struct step_counts c = {0};
    struct motion_planner *mp = make_planner(count_steps, &c);

    feed(mp, "G28");     // home all -- rebases position, no physical motion
    feed(mp, "G92 X5");  // rebase X's label to 5 -- still no physical motion

    CHECK("G28/G92 themselves never produce step events"
         , c.x == 0 && c.y == 0 && c.z == 0);

    motion_planner_destroy(mp);
}

static void
test_g92_rebases_position_so_the_next_move_distance_is_relative_to_it(void)
{
    // Move to X=10 (real motion, produces steps), then G92 X0 relabels
    // that same physical position as "0" without moving. A following
    // "G1 X10" is then another full 10mm of real motion (not a no-op),
    // because the target is now 10mm away from the *relabeled* current
    // position -- proving G92 rebased the internal position, not just
    // the reported one.
    struct step_counts c = {0};
    struct motion_planner *mp = make_planner(count_steps, &c);

    feed(mp, "G1 X10 F3000");
    long first_move_x = c.x;
    CHECK("first move to X10 produced steps", first_move_x > 0);

    feed(mp, "G92 X0"); // relabel current physical position as X=0

    feed(mp, "G1 X10 F3000"); // should be another real 10mm move
    long second_move_x = c.x - first_move_x;
    CHECK("after G92 X0, G1 X10 is real motion again (not a no-op)"
         , second_move_x > first_move_x / 2); // same order of magnitude as the first move

    motion_planner_destroy(mp);
}

static void
test_g28_rebases_selected_axis_to_zero_via_bare_letter(void)
{
    // "G28 X" -- gcode-parser doesn't capture the bare "X" as a param
    // (no number follows it), so this only works if motion-planner's
    // raw_args fallback scan is working. Move to (X10, Y20), home only
    // X, then move to (X10, Y20) again: X should be real motion again
    // (rebased to 0), Y should produce no further steps (already at 20,
    // untouched by the selective home).
    struct step_counts c = {0};
    struct motion_planner *mp = make_planner(count_steps, &c);

    feed(mp, "G1 X10 Y20 F3000");
    long x_after_first = c.x, y_after_first = c.y;
    CHECK("initial move produced both X and Y steps"
         , x_after_first > 0 && y_after_first > 0);

    feed(mp, "G28 X"); // bare letter, X only

    feed(mp, "G1 X10 Y20 F3000");
    CHECK("re-homed X axis moves again (bare-letter axis selection worked)"
         , c.x > x_after_first);
    CHECK("Y axis, not selected by \"G28 X\", produced no further steps"
         , c.y == y_after_first);

    motion_planner_destroy(mp);
}

static void
test_zero_distance_move_is_a_harmless_noop(void)
{
    struct step_counts c = {0};
    struct motion_planner *mp = make_planner(count_steps, &c);

    feed(mp, "G1 X0 Y0 Z0 F3000"); // already at the origin -- no move
    CHECK("a move to the current position produces no steps", c.x + c.y + c.z == 0);

    motion_planner_destroy(mp);
}

static void
test_time_cursor_advances(void)
{
    struct motion_planner *mp = make_planner(NULL, NULL);
    CHECK("time cursor starts at 0", motion_planner_get_time_cursor(mp) == 0.0);
    feed(mp, "G1 X60 F6000");
    CHECK("time cursor advances after a move"
         , motion_planner_get_time_cursor(mp) > 0.0);
    motion_planner_destroy(mp);
}

int
main(void)
{
    test_single_move_x_axis();
    test_multiple_short_moves_none_dropped();
    test_modal_axis_position_preserved();
    test_non_move_commands_ignored();
    test_g28_and_g92_produce_no_steps_themselves();
    test_g92_rebases_position_so_the_next_move_distance_is_relative_to_it();
    test_g28_rebases_selected_axis_to_zero_via_bare_letter();
    test_zero_distance_move_is_a_harmless_noop();
    test_time_cursor_advances();

    printf("\n%s (%d failure%s)\n"
          , g_failures ? "SOME TESTS FAILED" : "ALL TESTS PASSED"
          , g_failures, g_failures == 1 ? "" : "s");
    return g_failures ? 1 : 0;
}
