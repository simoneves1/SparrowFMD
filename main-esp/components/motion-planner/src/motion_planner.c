// See motion_planner.h.
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "motion_planner.h"
#include "trapq.h"
#include "itersolve.h"
#include "kin_cartesian.h"
#include "kinematics_steps.h"

// One shared trapq per toolhead (not one per axis -- matches real
// Klipper: all of a toolhead's steppers reference the same trapq, each
// stepper_kinematics just projects the shared move onto its own axis via
// axes_r, see trapq_append's parameters below).
struct axis_state {
    struct stepper_kinematics *sk;
    struct stepcompress *sc;
    char axis;
};

struct motion_planner {
    struct trapq *tq;
    struct axis_state x, y, z;

    float pos_x, pos_y, pos_z; // current commanded position, mm
    float feedrate_mm_s;       // sticky, converted from F (mm/min) on sight
    double time_cursor;        // print_time-space, seconds

    float max_velocity_mm_s;
    float max_accel_mm_s2;

    motion_planner_step_cb on_step;
    void *cb_ctx;
};

// kinematics_steps' callback signature doesn't carry which axis fired --
// each axis_state has its own struct stepcompress, so each needs its own
// trampoline closure to tag the axis before forwarding to the caller's
// single on_step callback.
struct step_trampoline_ctx {
    struct motion_planner *mp;
    char axis;
};

static void
step_trampoline(int dir, double time, void *ctx)
{
    struct step_trampoline_ctx *tc = ctx;
    if (!tc->mp->on_step)
        return;
    struct motion_planner_step_event ev = {.axis = tc->axis, .dir = dir, .time = time};
    tc->mp->on_step(&ev, tc->mp->cb_ctx);
}

// One trampoline per axis, allocated alongside the planner so its
// lifetime matches (kinematics_steps_alloc only takes a raw ctx pointer,
// it doesn't own/copy it).
struct motion_planner_priv {
    struct motion_planner pub;
    struct step_trampoline_ctx tramp_x, tramp_y, tramp_z;
};

struct motion_planner *
motion_planner_create(const struct motion_planner_config *cfg)
{
    struct motion_planner_priv *priv = malloc(sizeof(*priv));
    if (!priv)
        return NULL;
    memset(priv, 0, sizeof(*priv));
    struct motion_planner *mp = &priv->pub;

    mp->tq = trapq_alloc();
    if (!mp->tq) {
        free(priv);
        return NULL;
    }

    mp->x.axis = 'x';
    mp->y.axis = 'y';
    mp->z.axis = 'z';
    mp->x.sk = cartesian_stepper_alloc('x');
    mp->y.sk = cartesian_stepper_alloc('y');
    mp->z.sk = cartesian_stepper_alloc('z');
    if (!mp->x.sk || !mp->y.sk || !mp->z.sk) {
        trapq_free(mp->tq);
        free(mp->x.sk); free(mp->y.sk); free(mp->z.sk);
        free(priv);
        return NULL;
    }

    itersolve_set_trapq(mp->x.sk, mp->tq, cfg->step_dist_x);
    itersolve_set_trapq(mp->y.sk, mp->tq, cfg->step_dist_y);
    itersolve_set_trapq(mp->z.sk, mp->tq, cfg->step_dist_z);
    itersolve_set_position(mp->x.sk, 0.f, 0.f, 0.f);
    itersolve_set_position(mp->y.sk, 0.f, 0.f, 0.f);
    itersolve_set_position(mp->z.sk, 0.f, 0.f, 0.f);

    mp->on_step = cfg->on_step;
    mp->cb_ctx = cfg->cb_ctx;
    priv->tramp_x = (struct step_trampoline_ctx){.mp = mp, .axis = 'x'};
    priv->tramp_y = (struct step_trampoline_ctx){.mp = mp, .axis = 'y'};
    priv->tramp_z = (struct step_trampoline_ctx){.mp = mp, .axis = 'z'};
    mp->x.sc = kinematics_steps_alloc(step_trampoline, &priv->tramp_x);
    mp->y.sc = kinematics_steps_alloc(step_trampoline, &priv->tramp_y);
    mp->z.sc = kinematics_steps_alloc(step_trampoline, &priv->tramp_z);
    mp->x.sk->sc = mp->x.sc;
    mp->y.sk->sc = mp->y.sc;
    mp->z.sk->sc = mp->z.sc;

    mp->max_velocity_mm_s = cfg->max_velocity_mm_s;
    mp->max_accel_mm_s2 = cfg->max_accel_mm_s2;
    mp->feedrate_mm_s = cfg->max_velocity_mm_s; // sane default until an F is seen
    mp->time_cursor = 0.;

    return mp;
}

void
motion_planner_destroy(struct motion_planner *mp)
{
    if (!mp)
        return;
    kinematics_steps_free(mp->x.sc);
    kinematics_steps_free(mp->y.sc);
    kinematics_steps_free(mp->z.sc);
    free(mp->x.sk); free(mp->y.sk); free(mp->z.sk);
    trapq_free(mp->tq);
    // mp is the first member of motion_planner_priv, so this also frees
    // the trampoline contexts allocated alongside it.
    free(mp);
}

double
motion_planner_get_time_cursor(const struct motion_planner *mp)
{
    return mp->time_cursor;
}

static bool
queue_move(struct motion_planner *mp, float target_x, float target_y
          , float target_z)
{
    float dx = target_x - mp->pos_x;
    float dy = target_y - mp->pos_y;
    float dz = target_z - mp->pos_z;
    float distance = sqrtf(dx * dx + dy * dy + dz * dz);
    if (distance <= 0.f)
        return true; // no-op move, nothing to queue

    float axes_r_x = dx / distance, axes_r_y = dy / distance, axes_r_z = dz / distance;

    // Single-move trapezoid, start_v = end_v = 0 always -- see this
    // component's header comment on why (no lookahead queue yet).
    float v = mp->feedrate_mm_s > mp->max_velocity_mm_s
        ? mp->max_velocity_mm_s : mp->feedrate_mm_s;
    float a = mp->max_accel_mm_s2;
    float accel_t, cruise_t, decel_t, cruise_v;
    float accel_dist_to_reach_v = (v * v) / (2.f * a);
    if (2.f * accel_dist_to_reach_v >= distance) {
        // Triangular profile -- never reaches the target velocity.
        float peak_v = sqrtf(a * distance);
        accel_t = decel_t = peak_v / a;
        cruise_t = 0.f;
        cruise_v = peak_v;
    } else {
        accel_t = decel_t = v / a;
        float cruise_dist = distance - 2.f * accel_dist_to_reach_v;
        cruise_t = cruise_dist / v;
        cruise_v = v;
    }

    trapq_append(mp->tq, (float)mp->time_cursor, accel_t, cruise_t, decel_t
                , mp->pos_x, mp->pos_y, mp->pos_z
                , axes_r_x, axes_r_y, axes_r_z
                , 0.f, cruise_v, a);

    double move_duration = (double)accel_t + cruise_t + decel_t;
    mp->time_cursor += move_duration;
    mp->pos_x = target_x;
    mp->pos_y = target_y;
    mp->pos_z = target_z;

    // Flush steps for this move on every axis right away -- no lookahead
    // queue means no reason to defer, see this component's header comment.
    //
    // flush_time = time_cursor exactly, NOT time_cursor + some margin:
    // itersolve_generate_steps() stores flush_time into sk->last_flush_time
    // for the *next* call to resume from. The kinematics test harness
    // this was first written against only ever queued one isolated move
    // per run, so an arbitrary "+1s" margin there was harmless. Here,
    // moves queue back-to-back -- a margin bigger than the next move's
    // own duration would make the next call's last_flush_time look like
    // it already covers past that move's end, silently skipping its
    // steps entirely. No margin is needed for correctness within this
    // move either: itersolve_gen_steps_range() already clamps its
    // internal `end` to the move's own move_t regardless of how far past
    // it flush_time reaches.
    double flush_time = mp->time_cursor;
    itersolve_generate_steps(mp->x.sk, mp->x.sc, (float)flush_time);
    itersolve_generate_steps(mp->y.sk, mp->y.sc, (float)flush_time);
    itersolve_generate_steps(mp->z.sk, mp->z.sc, (float)flush_time);
    kinematics_steps_finalize(mp->x.sc);
    kinematics_steps_finalize(mp->y.sc);
    kinematics_steps_finalize(mp->z.sc);
    return true;
}

// Rebases the planner's and kinematics' internal position to (x, y, z)
// without queuing any motion -- shared by G28 and G92, see header
// comment for why both use this (G28 has no real endstop to seek here,
// so it's the same operation as G92 targeting 0).
static void
rebase_position(struct motion_planner *mp, float x, float y, float z)
{
    mp->pos_x = x;
    mp->pos_y = y;
    mp->pos_z = z;
    itersolve_set_position(mp->x.sk, x, y, z);
    itersolve_set_position(mp->y.sk, x, y, z);
    itersolve_set_position(mp->z.sk, x, y, z);
}

// Bare axis letters (no following number) don't get captured by
// gcode-parser's param collection -- they land in raw_args instead, see
// this component's header comment. This is the fallback scan for that
// form, e.g. "G28 X Y" or "G92 X Y".
static void
scan_raw_args_for_axes(const char *raw_args, bool *sel_x, bool *sel_y, bool *sel_z)
{
    for (const char *p = raw_args; *p; p++) {
        if (*p == 'X' || *p == 'x') *sel_x = true;
        else if (*p == 'Y' || *p == 'y') *sel_y = true;
        else if (*p == 'Z' || *p == 'z') *sel_z = true;
    }
}

bool
motion_planner_handle_gcode(struct motion_planner *mp
                            , const struct gcode_command *cmd)
{
    if (cmd->letter != 'G')
        return true; // not a G command -- ignored on purpose, see header comment

    if (cmd->code == 28 || cmd->code == 92) {
        const struct gcode_param *xp = gcode_find_param(cmd, 'X');
        const struct gcode_param *yp = gcode_find_param(cmd, 'Y');
        const struct gcode_param *zp = gcode_find_param(cmd, 'Z');
        bool sel_x = xp != NULL, sel_y = yp != NULL, sel_z = zp != NULL;
        if (!sel_x && !sel_y && !sel_z)
            scan_raw_args_for_axes(cmd->raw_args, &sel_x, &sel_y, &sel_z);

        // No axis letters at all (bare "G28"/"G92") selects every axis.
        if (!sel_x && !sel_y && !sel_z)
            sel_x = sel_y = sel_z = true;

        // G28 always rebases to 0 (no real endstop to seek toward, see
        // header comment); G92 rebases to the given value, or keeps the
        // axis's current value if only its bare letter was seen (no
        // number given, e.g. "G92 X" -- treat as "leave X where it is").
        float target_x = mp->pos_x, target_y = mp->pos_y, target_z = mp->pos_z;
        if (cmd->code == 28) {
            if (sel_x) target_x = 0.f;
            if (sel_y) target_y = 0.f;
            if (sel_z) target_z = 0.f;
        } else {
            if (xp) target_x = (float)xp->value;
            if (yp) target_y = (float)yp->value;
            if (zp) target_z = (float)zp->value;
        }
        rebase_position(mp, target_x, target_y, target_z);
        return true;
    }

    if (cmd->code != 0 && cmd->code != 1)
        return true; // not a move -- ignored on purpose, see header comment

    const struct gcode_param *fp = gcode_find_param(cmd, 'F');
    if (fp)
        mp->feedrate_mm_s = (float)(fp->value / 60.0); // mm/min -> mm/s

    const struct gcode_param *xp = gcode_find_param(cmd, 'X');
    const struct gcode_param *yp = gcode_find_param(cmd, 'Y');
    const struct gcode_param *zp = gcode_find_param(cmd, 'Z');
    float target_x = xp ? (float)xp->value : mp->pos_x;
    float target_y = yp ? (float)yp->value : mp->pos_y;
    float target_z = zp ? (float)zp->value : mp->pos_z;

    return queue_move(mp, target_x, target_y, target_z);
}
