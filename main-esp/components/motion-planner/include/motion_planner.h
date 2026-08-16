// Turns already-parsed G-code commands (gcode-parser's output) into
// queued kinematics moves, and reports the resulting stepper pulse
// events. This is the piece gcode-parser's own design note explicitly
// says doesn't belong there ("stays decoupled from kinematics on
// purpose") -- this component is that consumer.
//
// Deliberately minimal, matching this project's established "no
// lookahead" precedent from the kinematics module itself:
//   - G0/G1/G28/G92 are acted on. Everything else (M-codes, etc.) is
//     silently ignored -- this proves the gcode -> kinematics -> steps
//     chain works end to end, it is not a gcode interpreter.
//   - G28 (homing) does NOT perform real endstop-seeking motion -- this
//     project has no endstop input yet. It just rebases the planner's
//     (and kinematics') internal position to 0 for the selected axes,
//     the same way G92 rebases it to a caller-given value. Fine for
//     exercising the chain; a real homing routine needs real endstop
//     hardware to seek toward first.
//   - Both G28 and G92 only recognize axis letters that appear with an
//     explicit numeric value (e.g. "G92 X0") OR no axis letters at all
//     (home/reset everything) directly via gcode-parser's params --
//     gcode-parser's param collection does not capture bare letters
//     with no following number (real G28's usual "G28 X Y" form), a
//     known limitation of that parser (see its own header comment).
//     Bare letters land in `raw_args` instead of `params`, so this
//     module also scans `raw_args` for X/Y/Z characters as a fallback
//     to still support that form.
//   - No lookahead queue: every move decelerates fully to a stop before
//     the next one starts (start_v = end_v = 0 always), the same
//     trapezoid math a single Klipper move would use in isolation. Real
//     Klipper smooths velocity across queued moves and limits cornering
//     speed by junction angle -- this doesn't, on purpose, to keep the
//     first wiring pass tractable. Every move here will have a
//     dead-stop between moves, which is functionally correct but not
//     how a real print should feel; revisit once this base chain is
//     verified.
//   - G0/G1 are treated identically (no "rapid" vs "feed" distinction).
//   - F (feedrate) is sticky across commands, in mm/min per G-code
//     convention, converted to mm/s internally to match kinematics' units.
//   - X/Y/Z omitted from a G1 line keep their current value (standard
//     G-code "modal" behavior) -- a G1 with none of X/Y/Z/F present is a
//     no-op, not an error.
#ifndef MOTION_PLANNER_H
#define MOTION_PLANNER_H

#include <stdbool.h>
#include "gcode_parser.h"

struct motion_planner_step_event {
    char axis;  // 'x', 'y', or 'z'
    int dir;    // matches kinematics_steps.h's dir convention
    double time; // absolute print_time-space step time (seconds)
};

typedef void (*motion_planner_step_cb)(const struct motion_planner_step_event *ev
                                       , void *ctx);

struct motion_planner_config {
    float step_dist_x, step_dist_y, step_dist_z; // mm/step, per axis
    float max_velocity_mm_s;
    float max_accel_mm_s2;
    motion_planner_step_cb on_step; // may be NULL to discard step events
    void *cb_ctx;
};

struct motion_planner;

// Starting position is (0,0,0) -- call motion_planner_handle_gcode with
// a G92 if a caller ever needs to set a different origin.
struct motion_planner *motion_planner_create(const struct motion_planner_config *cfg);
void motion_planner_destroy(struct motion_planner *mp);

// Feeds one already-parsed command. Returns false only for a structurally
// malformed G0/G1 line (gcode-parser would normally have already
// rejected that before this ever saw it) -- an ignored (non-G0/G1/G28/
// G92) command returns true, since ignoring it is expected behavior,
// not an error.
bool motion_planner_handle_gcode(struct motion_planner *mp
                                 , const struct gcode_command *cmd);

// How far the queued moves extend, in the same print_time-space the
// kinematics module uses. Exposed for tests/diagnostics, not needed for
// normal use.
double motion_planner_get_time_cursor(const struct motion_planner *mp);

#endif // motion_planner.h
