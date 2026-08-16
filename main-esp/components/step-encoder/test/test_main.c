// Host-buildable unit tests for step-encoder. Build with plain gcc, no
// ESP-IDF required (one line, shown split here only for readability):
//   gcc -Wall -o step_encoder_test test_main.c ../src/step_encoder.c
//     -I ../include -I ../../klipper-host-protocol/include
//     -I ../../motion-planner/include -I ../../gcode-parser/include
//     -I ../../klipper-host-protocol/third_party/cJSON
//     ../../klipper-host-protocol/src/vlq.c
//     ../../klipper-host-protocol/src/msgblock.c
//     ../../klipper-host-protocol/src/identify.c
//     ../../klipper-host-protocol/src/dictionary.c
//     ../../klipper-host-protocol/src/msgtable.c
//     ../../klipper-host-protocol/src/message.c
//     ../../klipper-host-protocol/src/session.c
//     ../../klipper-host-protocol/third_party/puff/puff.c
//     ../../klipper-host-protocol/third_party/cJSON/cJSON.c -lm
#include <stdio.h>
#include <string.h>
#include "step_encoder.h"
#include "khp_message.h"

static int g_failures = 0;

#define CHECK(desc, cond) do { \
    if (cond) { \
        printf("PASS: %s\n", desc); \
    } else { \
        printf("FAIL: %s\n", desc); \
        g_failures++; \
    } \
} while (0)

// A mock dictionary using Klipper's real, stable queue_step/
// set_next_step_dir format strings (klippy/../src/stepper.c) -- the
// exact msgids here are arbitrary/mock (real ones are assigned per MCU
// build via the identify dictionary), but the format strings themselves
// are the real, documented Klipper wire convention.
static const char MOCK_DICT_JSON[] =
    "{"
    "\"version\":\"mock-1\","
    "\"build_versions\":\"gcc-12\","
    "\"commands\":{"
        "\"queue_step oid=%c interval=%u count=%hu add=%hi\":10,"
        "\"set_next_step_dir oid=%c dir=%c\":11"
    "},"
    "\"responses\":{},"
    "\"output\":{}"
    "}";

static const char INCOMPLETE_DICT_JSON[] =
    "{"
    "\"version\":\"mock-2\","
    "\"build_versions\":\"gcc-12\","
    "\"commands\":{"
        "\"queue_step oid=%c interval=%u count=%hu add=%hi\":10"
    "},"
    "\"responses\":{},"
    "\"output\":{}"
    "}";

static bool
decode_step_values(struct step_encoder_message *m
                   , const struct khp_msgtable *t, int32_t out[4])
{
    const struct khp_msg_entry *e = khp_msgtable_find_by_id(t, m->msgid);
    if (!e)
        return false;
    struct khp_param_list params;
    if (!khp_msgtable_lookup_params(t, e->format, &params))
        return false;
    struct khp_value values[4];
    bool ok = params.count == 4
        && khp_msg_decode(m->content, m->content_len, m->msgid, &params, values);
    if (ok)
        for (int i = 0; i < 4; i++)
            out[i] = values[i].i;
    khp_param_list_free(&params);
    return ok;
}

static bool
decode_dir_values(struct step_encoder_message *m
                  , const struct khp_msgtable *t, int32_t out[2])
{
    const struct khp_msg_entry *e = khp_msgtable_find_by_id(t, m->msgid);
    if (!e)
        return false;
    struct khp_param_list params;
    if (!khp_msgtable_lookup_params(t, e->format, &params))
        return false;
    struct khp_value values[2];
    bool ok = params.count == 2
        && khp_msg_decode(m->content, m->content_len, m->msgid, &params, values);
    if (ok)
        for (int i = 0; i < 2; i++)
            out[i] = values[i].i;
    khp_param_list_free(&params);
    return ok;
}

static void
test_create_rejects_incomplete_dictionary(void)
{
    struct khp_msgtable t;
    bool ok = khp_msgtable_parse(&t, INCOMPLETE_DICT_JSON
                                 , sizeof(INCOMPLETE_DICT_JSON) - 1);
    CHECK("incomplete dict parses", ok);
    if (!ok)
        return;

    struct step_encoder_config cfg = {
        .oid_x = 1, .oid_y = 2, .oid_z = 3,
        .mcu_freq_hz = 16000000.0,
        .table = &t,
    };
    struct step_encoder *enc = step_encoder_create(&cfg);
    CHECK("create fails without set_next_step_dir in dictionary", enc == NULL);
    step_encoder_destroy(enc);
    khp_msgtable_free(&t);
}

static void
test_first_step_emits_dir_then_queue_step(void)
{
    struct khp_msgtable t;
    CHECK("mock dict parses"
         , khp_msgtable_parse(&t, MOCK_DICT_JSON, sizeof(MOCK_DICT_JSON) - 1));

    struct step_encoder_config cfg = {
        .oid_x = 5, .oid_y = 6, .oid_z = 7,
        .mcu_freq_hz = 1000000.0, // 1 MHz -> 1 tick == 1 microsecond
        .table = &t,
    };
    struct step_encoder *enc = step_encoder_create(&cfg);
    CHECK("encoder created", enc != NULL);
    if (!enc) { khp_msgtable_free(&t); return; }

    struct motion_planner_step_event ev = { .axis = 'x', .dir = 1, .time = 0.000100 };
    struct step_encoder_message msgs[2];
    int n = step_encoder_encode_step(enc, &ev, msgs);
    CHECK("first step on an axis emits 2 messages (dir + queue_step)", n == 2);

    int32_t dir_vals[2];
    CHECK("dir message decodes"
         , decode_dir_values(&msgs[0], &t, dir_vals));
    CHECK("dir message has correct oid", dir_vals[0] == 5);
    CHECK("dir message has correct dir", dir_vals[1] == 1);

    int32_t step_vals[4];
    CHECK("queue_step message decodes"
         , decode_step_values(&msgs[1], &t, step_vals));
    CHECK("queue_step oid correct", step_vals[0] == 5);
    CHECK("queue_step interval == clock for first step (100us @ 1MHz = 100 ticks)"
         , step_vals[1] == 100);
    CHECK("queue_step count is always 1 (no run-length compression yet)"
         , step_vals[2] == 1);
    CHECK("queue_step add is always 0", step_vals[3] == 0);

    step_encoder_destroy(enc);
    khp_msgtable_free(&t);
}

static void
test_second_step_same_dir_omits_dir_message(void)
{
    struct khp_msgtable t;
    khp_msgtable_parse(&t, MOCK_DICT_JSON, sizeof(MOCK_DICT_JSON) - 1);
    struct step_encoder_config cfg = {
        .oid_x = 5, .oid_y = 6, .oid_z = 7,
        .mcu_freq_hz = 1000000.0,
        .table = &t,
    };
    struct step_encoder *enc = step_encoder_create(&cfg);

    struct motion_planner_step_event ev1 = { .axis = 'x', .dir = 1, .time = 0.000100 };
    struct step_encoder_message msgs[2];
    step_encoder_encode_step(enc, &ev1, msgs);

    struct motion_planner_step_event ev2 = { .axis = 'x', .dir = 1, .time = 0.000150 };
    int n = step_encoder_encode_step(enc, &ev2, msgs);
    CHECK("second step, same direction, emits only queue_step", n == 1);

    int32_t step_vals[4];
    decode_step_values(&msgs[0], &t, step_vals);
    CHECK("second step's interval is the delta since the last step (50us = 50 ticks)"
         , step_vals[1] == 50);

    step_encoder_destroy(enc);
    khp_msgtable_free(&t);
}

static void
test_direction_change_re_emits_dir_message(void)
{
    struct khp_msgtable t;
    khp_msgtable_parse(&t, MOCK_DICT_JSON, sizeof(MOCK_DICT_JSON) - 1);
    struct step_encoder_config cfg = {
        .oid_x = 5, .oid_y = 6, .oid_z = 7,
        .mcu_freq_hz = 1000000.0,
        .table = &t,
    };
    struct step_encoder *enc = step_encoder_create(&cfg);

    struct motion_planner_step_event ev1 = { .axis = 'y', .dir = 1, .time = 0.000100 };
    struct step_encoder_message msgs[2];
    step_encoder_encode_step(enc, &ev1, msgs);

    struct motion_planner_step_event ev2 = { .axis = 'y', .dir = 0, .time = 0.000200 };
    int n = step_encoder_encode_step(enc, &ev2, msgs);
    CHECK("direction change re-emits a dir message", n == 2);
    int32_t dir_vals[2];
    decode_dir_values(&msgs[0], &t, dir_vals);
    CHECK("re-emitted dir message carries the new direction", dir_vals[1] == 0);
    CHECK("re-emitted dir message uses the y axis oid", dir_vals[0] == 6);

    step_encoder_destroy(enc);
    khp_msgtable_free(&t);
}

static void
test_axes_use_their_own_oid_and_clock_independently(void)
{
    struct khp_msgtable t;
    khp_msgtable_parse(&t, MOCK_DICT_JSON, sizeof(MOCK_DICT_JSON) - 1);
    struct step_encoder_config cfg = {
        .oid_x = 5, .oid_y = 6, .oid_z = 7,
        .mcu_freq_hz = 1000000.0,
        .table = &t,
    };
    struct step_encoder *enc = step_encoder_create(&cfg);

    struct motion_planner_step_event evx = { .axis = 'x', .dir = 1, .time = 0.000500 };
    struct motion_planner_step_event evz = { .axis = 'z', .dir = 1, .time = 0.000010 };
    struct step_encoder_message msgs[2];

    step_encoder_encode_step(enc, &evx, msgs);
    int n = step_encoder_encode_step(enc, &evz, msgs);
    CHECK("z axis's first step is unaffected by x's clock (own axis state)"
         , n == 2);
    int32_t step_vals[4];
    decode_step_values(&msgs[1], &t, step_vals);
    CHECK("z step uses z's oid, not x's", step_vals[0] == 7);
    CHECK("z step's interval is its own absolute clock (10us = 10 ticks)"
         , step_vals[1] == 10);

    step_encoder_destroy(enc);
    khp_msgtable_free(&t);
}

static void
test_unknown_axis_rejected(void)
{
    struct khp_msgtable t;
    khp_msgtable_parse(&t, MOCK_DICT_JSON, sizeof(MOCK_DICT_JSON) - 1);
    struct step_encoder_config cfg = {
        .oid_x = 5, .oid_y = 6, .oid_z = 7,
        .mcu_freq_hz = 1000000.0,
        .table = &t,
    };
    struct step_encoder *enc = step_encoder_create(&cfg);

    struct motion_planner_step_event ev = { .axis = 'e', .dir = 1, .time = 0.0001 };
    struct step_encoder_message msgs[2];
    int n = step_encoder_encode_step(enc, &ev, msgs);
    CHECK("unrecognized axis char is rejected", n == -1);

    step_encoder_destroy(enc);
    khp_msgtable_free(&t);
}

static void
test_time_going_backwards_on_an_axis_rejected(void)
{
    struct khp_msgtable t;
    khp_msgtable_parse(&t, MOCK_DICT_JSON, sizeof(MOCK_DICT_JSON) - 1);
    struct step_encoder_config cfg = {
        .oid_x = 5, .oid_y = 6, .oid_z = 7,
        .mcu_freq_hz = 1000000.0,
        .table = &t,
    };
    struct step_encoder *enc = step_encoder_create(&cfg);

    struct motion_planner_step_event ev1 = { .axis = 'x', .dir = 1, .time = 0.000200 };
    struct motion_planner_step_event ev2 = { .axis = 'x', .dir = 1, .time = 0.000100 };
    struct step_encoder_message msgs[2];
    step_encoder_encode_step(enc, &ev1, msgs);
    int n = step_encoder_encode_step(enc, &ev2, msgs);
    CHECK("a step event earlier than the axis's last step is rejected", n == -1);

    step_encoder_destroy(enc);
    khp_msgtable_free(&t);
}

int
main(void)
{
    test_create_rejects_incomplete_dictionary();
    test_first_step_emits_dir_then_queue_step();
    test_second_step_same_dir_omits_dir_message();
    test_direction_change_re_emits_dir_message();
    test_axes_use_their_own_oid_and_clock_independently();
    test_unknown_axis_rejected();
    test_time_going_backwards_on_an_axis_rejected();

    printf("\n%d failure(s)\n", g_failures);
    return g_failures ? 1 : 0;
}
