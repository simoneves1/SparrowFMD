// Host-buildable unit tests for web-ui's web_api.
// Build with any C compiler, no ESP-IDF required:
//   gcc -Wall -I../include -I../third_party/cJSON -o web_api_test
//   test_main.c ../src/web_api.c ../third_party/cJSON/cJSON.c
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "web_api.h"

static int g_failures = 0;

#define CHECK(desc, cond) do { \
    if (cond) { \
        printf("PASS: %s\n", desc); \
    } else { \
        printf("FAIL: %s\n", desc); \
        g_failures++; \
    } \
} while (0)

static bool
dbl_eq(double a, double b)
{
    return fabs(a - b) < 1e-9;
}

static void
test_status_roundtrip(void)
{
    struct web_status s = {
        .state = WEB_STATE_PRINTING, .hotend_temp = 210.5
        , .hotend_target = 210.0, .bed_temp = 60.0, .bed_target = 60.0
        , .progress_percent = 42, .elapsed_s = 3723,
    };
    char *json = web_status_to_json(&s);
    CHECK("status encode produces a string", json != NULL);
    if (!json)
        return;

    CHECK("status JSON contains the expected type/state markers"
         , strstr(json, "\"type\":\"status\"") != NULL
           && strstr(json, "\"state\":\"printing\"") != NULL);

    struct web_status out;
    bool ok = web_status_from_json(json, strlen(json), &out);
    CHECK("status decode succeeds", ok);
    CHECK("status roundtrip preserves every field"
         , ok && out.state == s.state && dbl_eq(out.hotend_temp, s.hotend_temp)
           && dbl_eq(out.hotend_target, s.hotend_target)
           && dbl_eq(out.bed_temp, s.bed_temp)
           && dbl_eq(out.bed_target, s.bed_target)
           && out.progress_percent == s.progress_percent
           && out.elapsed_s == s.elapsed_s);

    CHECK("web_msg_type_of identifies a status message"
         , web_msg_type_of(json, strlen(json)) == WEB_MSG_STATUS);

    web_json_free(json);
}

static void
test_control_command_start_roundtrip(void)
{
    struct web_control_command c = {.command = WEB_CMD_START};
    char *json = web_control_command_to_json(&c);
    CHECK("start command encodes", json != NULL);
    if (!json)
        return;

    CHECK("start command JSON has no jog fields (not a jog command)"
         , strstr(json, "jog_dx") == NULL);

    struct web_control_command out;
    bool ok = web_control_command_from_json(json, strlen(json), &out);
    CHECK("start command decodes and roundtrips", ok && out.command == WEB_CMD_START);
    CHECK("web_msg_type_of identifies a command message"
         , web_msg_type_of(json, strlen(json)) == WEB_MSG_COMMAND);

    web_json_free(json);
}

static void
test_control_command_jog_roundtrip(void)
{
    struct web_control_command c = {
        .command = WEB_CMD_JOG, .jog_dx = -10.5, .jog_dy = 5.0
        , .jog_dz = 0.0, .jog_feedrate = 3000.0,
    };
    char *json = web_control_command_to_json(&c);
    CHECK("jog command encodes", json != NULL);
    if (!json)
        return;

    struct web_control_command out;
    bool ok = web_control_command_from_json(json, strlen(json), &out);
    CHECK("jog command decodes and roundtrips every field, incl. negative"
         , ok && out.command == WEB_CMD_JOG && dbl_eq(out.jog_dx, c.jog_dx)
           && dbl_eq(out.jog_dy, c.jog_dy) && dbl_eq(out.jog_dz, c.jog_dz)
           && dbl_eq(out.jog_feedrate, c.jog_feedrate));

    web_json_free(json);
}

static void
test_msg_type_of_edge_cases(void)
{
    CHECK("web_msg_type_of on malformed JSON is UNKNOWN"
         , web_msg_type_of("not json", 8) == WEB_MSG_UNKNOWN);
    static const char no_type[] = "{\"foo\":1}";
    CHECK("web_msg_type_of on JSON missing 'type' is UNKNOWN"
         , web_msg_type_of(no_type, sizeof(no_type) - 1) == WEB_MSG_UNKNOWN);
    static const char bad_type[] = "{\"type\":\"bogus\"}";
    CHECK("web_msg_type_of on an unrecognized 'type' value is UNKNOWN"
         , web_msg_type_of(bad_type, sizeof(bad_type) - 1) == WEB_MSG_UNKNOWN);
}

static void
test_decode_rejects_wrong_type(void)
{
    struct web_control_command c = {.command = WEB_CMD_STOP};
    char *json = web_control_command_to_json(&c);
    struct web_status out;
    CHECK("web_status_from_json rejects a 'command' message"
         , !web_status_from_json(json, strlen(json), &out));
    web_json_free(json);
}

static void
test_decode_rejects_unrecognized_enum_value(void)
{
    static const char bad_state[] =
        "{\"type\":\"status\",\"state\":\"exploding\",\"hotend_temp\":0,"
        "\"hotend_target\":0,\"bed_temp\":0,\"bed_target\":0,"
        "\"progress\":0,\"elapsed_s\":0}";
    struct web_status out;
    CHECK("status decode rejects an unrecognized state name"
         , !web_status_from_json(bad_state, sizeof(bad_state) - 1, &out));

    static const char bad_cmd[] = "{\"type\":\"command\",\"command\":\"explode\"}";
    struct web_control_command cout;
    CHECK("command decode rejects an unrecognized command name"
         , !web_control_command_from_json(bad_cmd, sizeof(bad_cmd) - 1, &cout));
}

static void
test_decode_rejects_missing_fields(void)
{
    static const char incomplete[] =
        "{\"type\":\"status\",\"state\":\"idle\",\"hotend_temp\":0}";
    struct web_status out;
    CHECK("status decode rejects a message missing required fields"
         , !web_status_from_json(incomplete, sizeof(incomplete) - 1, &out));
}

int
main(void)
{
    test_status_roundtrip();
    test_control_command_start_roundtrip();
    test_control_command_jog_roundtrip();
    test_msg_type_of_edge_cases();
    test_decode_rejects_wrong_type();
    test_decode_rejects_unrecognized_enum_value();
    test_decode_rejects_missing_fields();

    printf("\n%s (%d failure%s)\n"
          , g_failures ? "SOME TESTS FAILED" : "ALL TESTS PASSED"
          , g_failures, g_failures == 1 ? "" : "s");
    return g_failures ? 1 : 0;
}
