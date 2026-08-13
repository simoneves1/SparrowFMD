#ifndef TRAPQ_H
#define TRAPQ_H

#include "list.h" // list_node

struct coord {
    union {
        struct {
            float x, y, z;
        };
        float axis[3];
    };
};

struct move {
    float print_time, move_t;
    float start_v, half_accel;
    struct coord start_pos, axes_r;

    struct list_node node;
};

struct trapq {
    struct list_head moves, history;
};

struct pull_move {
    float print_time, move_t;
    float start_v, accel;
    float start_x, start_y, start_z;
    float x_r, y_r, z_r;
};

struct move *move_alloc(void);
float move_get_distance(struct move *m, float move_time);
struct coord move_get_coord(struct move *m, float move_time);
struct trapq *trapq_alloc(void);
void trapq_free(struct trapq *tq);
void trapq_check_sentinels(struct trapq *tq);
void trapq_add_move(struct trapq *tq, struct move *m);
void trapq_append(struct trapq *tq, float print_time
                  , float accel_t, float cruise_t, float decel_t
                  , float start_pos_x, float start_pos_y, float start_pos_z
                  , float axes_r_x, float axes_r_y, float axes_r_z
                  , float start_v, float cruise_v, float accel);
void trapq_finalize_moves(struct trapq *tq, float print_time
                          , float clear_history_time);
void trapq_set_position(struct trapq *tq, float print_time
                        , float pos_x, float pos_y, float pos_z);
int trapq_extract_old(struct trapq *tq, struct pull_move *p, int max
                      , float start_time, float end_time);

#endif // trapq.h
