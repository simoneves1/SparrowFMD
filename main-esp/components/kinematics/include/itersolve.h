#ifndef ITERSOLVE_H
#define ITERSOLVE_H

#include <stdint.h> // int32_t

enum {
    AF_X = 1 << 0, AF_Y = 1 << 1, AF_Z = 1 << 2,
};

struct stepper_kinematics;
struct move;
typedef float (*sk_calc_callback)(struct stepper_kinematics *sk, struct move *m
                                   , float move_time);
typedef void (*sk_post_callback)(struct stepper_kinematics *sk);
struct stepper_kinematics {
    float step_dist, commanded_pos;
    struct stepcompress *sc;

    float last_flush_time, last_move_time;
    struct trapq *tq;
    int active_flags;
    float gen_steps_pre_active, gen_steps_post_active;

    sk_calc_callback calc_position_cb;
    sk_post_callback post_cb;
};

int32_t itersolve_generate_steps(struct stepper_kinematics *sk
                                 , struct stepcompress *sc, float flush_time);
float itersolve_check_active(struct stepper_kinematics *sk, float flush_time);
int32_t itersolve_is_active_axis(struct stepper_kinematics *sk, char axis);
void itersolve_set_trapq(struct stepper_kinematics *sk, struct trapq *tq
                         , float step_dist);
struct trapq *itersolve_get_trapq(struct stepper_kinematics *sk);
float itersolve_calc_position_from_coord(struct stepper_kinematics *sk
                                          , float x, float y, float z);
void itersolve_set_position(struct stepper_kinematics *sk
                            , float x, float y, float z);
float itersolve_get_commanded_pos(struct stepper_kinematics *sk);
float itersolve_get_gen_steps_pre_active(struct stepper_kinematics *sk);
float itersolve_get_gen_steps_post_active(struct stepper_kinematics *sk);

#endif // itersolve.h
