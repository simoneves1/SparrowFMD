#include "step_encoder.h"

#include <stdlib.h>
#include <math.h>
#include "khp_message.h"

struct axis_state {
    uint8_t oid;
    bool have_clock;
    uint32_t last_clock;
    int last_dir;
};

struct step_encoder {
    double mcu_freq_hz;

    uint32_t queue_step_msgid;
    struct khp_param_list queue_step_params;

    uint32_t dir_msgid;
    struct khp_param_list dir_params;

    struct axis_state x, y, z;
};

struct step_encoder *
step_encoder_create(const struct step_encoder_config *cfg)
{
    const struct khp_msg_entry *qs = khp_msgtable_find_by_name(cfg->table, "queue_step");
    const struct khp_msg_entry *sd = khp_msgtable_find_by_name(cfg->table, "set_next_step_dir");
    if (!qs || !sd)
        return NULL;

    struct step_encoder *enc = calloc(1, sizeof(*enc));
    if (!enc)
        return NULL;

    if (!khp_msgtable_lookup_params(cfg->table, qs->format, &enc->queue_step_params)
        || enc->queue_step_params.count != 4) {
        free(enc);
        return NULL;
    }
    if (!khp_msgtable_lookup_params(cfg->table, sd->format, &enc->dir_params)
        || enc->dir_params.count != 2) {
        khp_param_list_free(&enc->queue_step_params);
        free(enc);
        return NULL;
    }

    enc->queue_step_msgid = qs->msgid;
    enc->dir_msgid = sd->msgid;
    enc->mcu_freq_hz = cfg->mcu_freq_hz;

    enc->x.oid = cfg->oid_x;
    enc->y.oid = cfg->oid_y;
    enc->z.oid = cfg->oid_z;
    enc->x.last_dir = enc->y.last_dir = enc->z.last_dir = -1;

    return enc;
}

void
step_encoder_destroy(struct step_encoder *enc)
{
    if (!enc)
        return;
    khp_param_list_free(&enc->queue_step_params);
    khp_param_list_free(&enc->dir_params);
    free(enc);
}

static struct axis_state *
axis_state_for(struct step_encoder *enc, char axis)
{
    switch (axis) {
    case 'x': return &enc->x;
    case 'y': return &enc->y;
    case 'z': return &enc->z;
    default: return NULL;
    }
}

int
step_encoder_encode_step(struct step_encoder *enc
                         , const struct motion_planner_step_event *ev
                         , struct step_encoder_message out[2])
{
    struct axis_state *a = axis_state_for(enc, ev->axis);
    if (!a)
        return -1;

    uint32_t clock = (uint32_t)llround(ev->time * enc->mcu_freq_hz);

    int n = 0;

    if (!a->have_clock || a->last_dir != ev->dir) {
        struct khp_value dir_values[2] = {
            { .i = (int32_t)a->oid },
            { .i = (int32_t)ev->dir },
        };
        struct step_encoder_message *m = &out[n];
        m->msgid = enc->dir_msgid;
        if (!khp_msg_encode(m->content, sizeof(m->content), &m->content_len
                            , enc->dir_msgid, &enc->dir_params, dir_values))
            return -1;
        n++;
        a->last_dir = ev->dir;
    }

    if (a->have_clock && clock < a->last_clock)
        return -1; // step events must be non-decreasing in time per axis
    uint32_t interval = a->have_clock ? clock - a->last_clock : clock;

    struct khp_value step_values[4] = {
        { .i = (int32_t)a->oid },
        { .i = (int32_t)interval }, // count=1, so this step's own interval
        { .i = 1 },                 // count -- always 1, see header note
        { .i = 0 },                 // add -- always 0, no run compression
    };
    struct step_encoder_message *m = &out[n];
    m->msgid = enc->queue_step_msgid;
    if (!khp_msg_encode(m->content, sizeof(m->content), &m->content_len
                        , enc->queue_step_msgid, &enc->queue_step_params, step_values))
        return -1;
    n++;

    a->last_clock = clock;
    a->have_clock = true;

    return n;
}
