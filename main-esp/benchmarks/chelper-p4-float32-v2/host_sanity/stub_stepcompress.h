#ifndef STUB_STEPCOMPRESS_H
#define STUB_STEPCOMPRESS_H
// Query API for the host-sanity harness's stand-in stepcompress -- these
// are NOT part of Klipper's real stepcompress.h, just a way for main.c to
// read back what itersolve fed into stepcompress_append() during a run.

struct stepcompress;

// itersolve_gen_steps_range() calls stepcompress_append() multiple times
// while refining the SAME physical step (secant-method time refinement),
// not once per step -- the real stepcompress.c handles this via its MCU
// step-queue's ability to revise its own tail entry. This stub instead
// coalesces same-step refinements internally: consecutive append() calls
// within COALESCE_THRESHOLD seconds of each other overwrite a single
// pending slot; a call further away flushes the previous pending step as
// committed and starts a new one. Call this once after
// itersolve_generate_steps() returns to flush any still-pending step.
void stepcompress_finalize(struct stepcompress *sc);

long stepcompress_get_step_count(struct stepcompress *sc);
long stepcompress_get_nonmonotonic_count(struct stepcompress *sc);
long stepcompress_get_zero_delta_count(struct stepcompress *sc);
double stepcompress_get_min_positive_delta(struct stepcompress *sc);
double stepcompress_get_max_delta(struct stepcompress *sc);
double stepcompress_get_first_time(struct stepcompress *sc);
double stepcompress_get_last_time(struct stepcompress *sc);

#endif // stub_stepcompress.h
