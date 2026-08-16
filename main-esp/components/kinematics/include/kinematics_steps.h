// Minimal stand-in for Klipper's real stepcompress.c, providing exactly
// the interface itersolve.c's vendored code expects (see src/stepcompress.h):
// stepcompress_get_step_dir/commit/append. Klipper's real stepcompress.c
// does MCU step-queue run-length compression, clock-tick conversion, and
// position-history tracking for the wire protocol -- none of that exists
// here yet. This module's only job is: coalesce itersolve's repeated
// same-step refinement calls (see the note below) into one committed
// step event per physical step, and hand each one to a caller-supplied
// callback. Turning those callback events into real klipper-host-protocol
// queue_step messages and sending them to a real MCU board is future
// work -- this is the seam that future work plugs into.
//
// itersolve_gen_steps_range() calls stepcompress_append() several times
// while refining the time of a SINGLE physical step (secant-method
// bisection) before settling -- the real stepcompress.c handles this via
// its MCU step-queue's ability to revise its own tail entry. This module
// approximates that by coalescing: append() calls landing within
// KINEMATICS_STEPS_COALESCE_THRESHOLD seconds of the currently-pending
// step overwrite it rather than starting a new one; a call further away
// commits the pending step (firing the callback) and starts a new one.
// This was found and fixed the hard way during the original float32
// prototype: an earlier version without coalescing counted every
// refinement as a separate step and measured ~2x the physically correct
// count -- confirmed as a stub/module bug, not a kinematics bug, by
// reproducing it against unmodified double-precision chelper sources too.
#ifndef KINEMATICS_STEPS_H
#define KINEMATICS_STEPS_H

#include <stdint.h>

struct stepcompress;

// Called once per committed step, in time order. dir matches Klipper's
// own step_dir convention (see stepcompress_get_step_dir in
// src/stepcompress.h); time is the absolute print_time-space step time
// (seconds). May be called from within kinematics_steps_finalize() for
// the last pending step of a run, not just during generation.
typedef void (*kinematics_step_cb)(int dir, double time, void *ctx);

// One instance per stepper axis, matching Klipper's own one-stepcompress-
// per-stepper convention (see itersolve_set_trapq's sc parameter).
struct stepcompress *kinematics_steps_alloc(kinematics_step_cb cb, void *cb_ctx);
void kinematics_steps_free(struct stepcompress *sc);

// Flushes any still-pending (uncommitted) step, firing cb for it if one
// is pending. Call after every itersolve_generate_steps() call that
// might be the last one for a while -- a pending step won't otherwise
// be reported until a later append() call lands far enough away to
// force it out.
void kinematics_steps_finalize(struct stepcompress *sc);

#endif // kinematics_steps.h
