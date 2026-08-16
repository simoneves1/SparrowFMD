// See kinematics_steps.h.
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "stepcompress.h"
#include "kinematics_steps.h"

#define KINEMATICS_STEPS_COALESCE_THRESHOLD 0.00002 // 20 microseconds

struct stepcompress {
    kinematics_step_cb cb;
    void *cb_ctx;

    int pending_valid;
    int pending_dir;
    double pending_time;
};

static void
commit_pending(struct stepcompress *sc)
{
    if (!sc->pending_valid)
        return;
    if (sc->cb)
        sc->cb(sc->pending_dir, sc->pending_time, sc->cb_ctx);
    sc->pending_valid = 0;
}

struct stepcompress *
kinematics_steps_alloc(kinematics_step_cb cb, void *cb_ctx)
{
    struct stepcompress *sc = malloc(sizeof(*sc));
    if (!sc)
        return NULL;
    memset(sc, 0, sizeof(*sc));
    sc->cb = cb;
    sc->cb_ctx = cb_ctx;
    return sc;
}

void
kinematics_steps_free(struct stepcompress *sc)
{
    free(sc);
}

int
stepcompress_get_step_dir(struct stepcompress *sc)
{
    return sc->pending_dir;
}

int
stepcompress_commit(struct stepcompress *sc)
{
    // Real semantics: mark everything queued so far as safe from
    // rollback on the MCU side -- doesn't necessarily mean the
    // in-progress step being refined is "done" (more append() calls for
    // it may still follow). Not modeled here beyond letting append()'s
    // coalescing do the work; see kinematics_steps.h's top comment.
    return 0;
}

int
stepcompress_append(struct stepcompress *sc, int sdir
                    , double print_time, double step_time)
{
    double t = print_time + step_time;
    if (sc->pending_valid
        && fabs(t - sc->pending_time) < KINEMATICS_STEPS_COALESCE_THRESHOLD) {
        // Refinement of the step already pending -- overwrite, don't
        // count as a second step.
        sc->pending_time = t;
        sc->pending_dir = sdir;
    } else {
        commit_pending(sc);
        sc->pending_time = t;
        sc->pending_dir = sdir;
        sc->pending_valid = 1;
    }
    return 0;
}

void
kinematics_steps_finalize(struct stepcompress *sc)
{
    commit_pending(sc);
}
