// Turns motion-planner's per-axis step events into real Klipper wire
// protocol message content bytes (queue_step / set_next_step_dir),
// using a khp_msgtable the same way klipper-host-protocol's own tests
// build one -- normally that table comes from a real MCU's identify
// dictionary, here the caller supplies one (a real board's dictionary
// once hardware exists, a test/mock one until then).
//
// Scope note (see README.md for the full story): this emits one
// queue_step message per step event, always count=1 add=0. Real
// Klipper's queue_step is a run-length-encoded format (interval/count/
// add covers a whole run of evenly-spaced steps) -- producing that
// requires stepcompress.c's real compression algorithm, which this
// project's kinematics_steps.h explicitly does not implement either.
// count=1 is a legitimate, valid special case of the real wire format
// (a real MCU accepts and executes it correctly), just not compressed.
#ifndef STEP_ENCODER_H
#define STEP_ENCODER_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "khp_msgtable.h"
#include "motion_planner.h"

struct step_encoder_config {
    uint8_t oid_x, oid_y, oid_z; // per-axis stepper object id, as assigned
                                 // by the MCU's identify config -- caller's
                                 // responsibility to know these
    double mcu_freq_hz;         // MCU clock rate steps are quantized to;
                                 // real value comes from the MCU's config,
                                 // not something this repo can hardcode
    const struct khp_msgtable *table; // must outlive the encoder; must
                                       // contain "queue_step" and
                                       // "set_next_step_dir" commands
};

struct step_encoder;

// Returns NULL if table is missing queue_step/set_next_step_dir, or
// either has a param list this encoder doesn't understand.
struct step_encoder *step_encoder_create(const struct step_encoder_config *cfg);
void step_encoder_destroy(struct step_encoder *enc);

#define STEP_ENCODER_MAX_CONTENT 16

struct step_encoder_message {
    uint32_t msgid;
    uint8_t content[STEP_ENCODER_MAX_CONTENT];
    size_t content_len;
};

// Encodes the message(s) needed to transmit one motion_planner step
// event: an optional set_next_step_dir (only emitted the first time an
// axis is seen, or when its direction changes since the last event on
// that axis) followed by a queue_step. Writes up to 2 entries into out
// and returns how many were written (1 or 2), or -1 on error (unknown
// axis char, or khp_msg_encode failed -- shouldn't happen for these two
// small fixed-shape messages unless STEP_ENCODER_MAX_CONTENT is wrong).
int step_encoder_encode_step(struct step_encoder *enc
                             , const struct motion_planner_step_event *ev
                             , struct step_encoder_message out[2]);

#endif // step_encoder.h
