// Minimal stand-in for Klipper's real stepcompress.c -- see stub_stepcompress.h.
// Same coalescing design as the first float32 prototype's harness (that
// prototype's directory was deleted per this project's spike convention,
// but this stub's logic is unchanged, ported forward as-is): itersolve
// calls stepcompress_append() several times while refining ONE physical
// step's time; a 20-microsecond coalescing window collapses those into a
// single logical step instead of over-counting refinements as separate
// steps.

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "stepcompress.h"
#include "stub_stepcompress.h"

#define COALESCE_THRESHOLD 0.00002 // 20 microseconds

struct stepcompress {
    int pending_valid;
    int pending_dir;
    double pending_time;

    int has_committed;
    long step_count;
    double prev_time;
    long nonmonotonic_count;
    long zero_delta_count;
    double min_positive_delta;
    double max_delta;
    double first_time, last_time;
};

static void
commit_pending(struct stepcompress *sc)
{
    if (!sc->pending_valid)
        return;
    double t = sc->pending_time;
    if (sc->has_committed) {
        double delta = t - sc->prev_time;
        if (delta <= 0.)
            sc->nonmonotonic_count++;
        if (delta == 0.)
            sc->zero_delta_count++;
        if (delta > 0. && (sc->min_positive_delta < 0.
                            || delta < sc->min_positive_delta))
            sc->min_positive_delta = delta;
        if (delta > sc->max_delta)
            sc->max_delta = delta;
    } else {
        sc->first_time = t;
        sc->has_committed = 1;
    }
    sc->prev_time = t;
    sc->last_time = t;
    sc->step_count++;
    sc->pending_valid = 0;
}

struct stepcompress *
stepcompress_alloc(struct list_head *msg_queue)
{
    struct stepcompress *sc = malloc(sizeof(*sc));
    memset(sc, 0, sizeof(*sc));
    sc->min_positive_delta = -1.;
    return sc;
}

void
stepcompress_free(struct stepcompress *sc)
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
    return 0;
}

int
stepcompress_append(struct stepcompress *sc, int sdir
                    , double print_time, double step_time)
{
    double t = print_time + step_time;
    if (sc->pending_valid && fabs(t - sc->pending_time) < COALESCE_THRESHOLD) {
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
stepcompress_finalize(struct stepcompress *sc)
{
    commit_pending(sc);
}

long stepcompress_get_step_count(struct stepcompress *sc)
{ return sc->step_count; }
long stepcompress_get_nonmonotonic_count(struct stepcompress *sc)
{ return sc->nonmonotonic_count; }
long stepcompress_get_zero_delta_count(struct stepcompress *sc)
{ return sc->zero_delta_count; }
double stepcompress_get_min_positive_delta(struct stepcompress *sc)
{ return sc->min_positive_delta; }
double stepcompress_get_max_delta(struct stepcompress *sc)
{ return sc->max_delta; }
double stepcompress_get_first_time(struct stepcompress *sc)
{ return sc->first_time; }
double stepcompress_get_last_time(struct stepcompress *sc)
{ return sc->last_time; }
