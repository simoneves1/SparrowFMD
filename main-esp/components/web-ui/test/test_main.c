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
        , .progress_percent = 42, .elapsed_s = 3723
        , .filename = "benchy.gcode", .layer_current = 87
        , .layer_total = 214, .remaining_s = 2280
        , .speed_factor = 120.0, .flow_factor = 95.0, .z_offset = -0.05,
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
           && out.elapsed_s == s.elapsed_s
           && strcmp(out.filename, s.filename) == 0
           && out.layer_current == s.layer_current
           && out.layer_total == s.layer_total
           && out.remaining_s == s.remaining_s
           && dbl_eq(out.speed_factor, s.speed_factor)
           && dbl_eq(out.flow_factor, s.flow_factor)
           && dbl_eq(out.z_offset, s.z_offset));

    CHECK("web_msg_type_of identifies a status message"
         , web_msg_type_of(json, strlen(json)) == WEB_MSG_STATUS);

    web_json_free(json);
}

static void
test_status_filename_truncation_on_decode(void)
{
    // 40 'x's -- longer than WEB_STATUS_FILENAME_LEN(24)-1 -- decode must
    // truncate and still null-terminate rather than overflow out->filename.
    static const char long_name_json[] =
        "{\"type\":\"status\",\"state\":\"idle\",\"hotend_temp\":0,"
        "\"hotend_target\":0,\"bed_temp\":0,\"bed_target\":0,"
        "\"progress\":0,\"elapsed_s\":0,"
        "\"filename\":\"xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx\","
        "\"layer_current\":0,\"layer_total\":0,\"remaining_s\":0,"
        "\"speed_factor\":100,\"flow_factor\":100,\"z_offset\":0}";
    struct web_status out;
    bool ok = web_status_from_json(long_name_json, sizeof(long_name_json) - 1
                                   , &out);
    CHECK("status decode truncates an over-long filename safely"
         , ok && strlen(out.filename) == WEB_STATUS_FILENAME_LEN - 1);
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
test_control_command_filament_roundtrip(void)
{
    struct web_control_command c = {
        .command = WEB_CMD_FILAMENT_LOAD, .jog_dz = 50.0
        , .jog_feedrate = 300.0,
    };
    char *json = web_control_command_to_json(&c);
    CHECK("filament_load command encodes", json != NULL);
    if (!json)
        return;

    CHECK("filament_load JSON has no jog_dx/jog_dy (not a jog command)"
         , strstr(json, "jog_dx") == NULL && strstr(json, "jog_dy") == NULL);

    struct web_control_command out;
    bool ok = web_control_command_from_json(json, strlen(json), &out);
    CHECK("filament_load decodes and roundtrips its move distance/feedrate"
         , ok && out.command == WEB_CMD_FILAMENT_LOAD
           && dbl_eq(out.jog_dz, c.jog_dz)
           && dbl_eq(out.jog_feedrate, c.jog_feedrate));

    web_json_free(json);
}

static void
test_control_command_temp_set_roundtrip(void)
{
    struct web_control_command c = {
        .command = WEB_CMD_SET_HOTEND_TEMP, .target_temp = 210.0,
    };
    char *json = web_control_command_to_json(&c);
    CHECK("set_hotend_temp command encodes", json != NULL);
    if (!json)
        return;

    struct web_control_command out;
    bool ok = web_control_command_from_json(json, strlen(json), &out);
    CHECK("set_hotend_temp decodes and roundtrips target_temp"
         , ok && out.command == WEB_CMD_SET_HOTEND_TEMP
           && dbl_eq(out.target_temp, c.target_temp));

    web_json_free(json);
}

static void
test_control_command_start_with_filename_roundtrip(void)
{
    struct web_control_command c = {.command = WEB_CMD_START};
    strncpy(c.filename, "benchy.gcode", sizeof(c.filename) - 1);
    char *json = web_control_command_to_json(&c);
    CHECK("start-with-filename command encodes", json != NULL);
    if (!json)
        return;

    struct web_control_command out;
    bool ok = web_control_command_from_json(json, strlen(json), &out);
    CHECK("start-with-filename decodes and roundtrips filename"
         , ok && out.command == WEB_CMD_START
           && strcmp(out.filename, "benchy.gcode") == 0);

    web_json_free(json);
}

static void
test_control_command_start_without_filename(void)
{
    struct web_control_command c = {.command = WEB_CMD_START};
    char *json = web_control_command_to_json(&c);
    CHECK("plain start command encodes", json != NULL);
    if (!json)
        return;

    CHECK("plain start command JSON omits filename entirely"
         , strstr(json, "filename") == NULL);

    struct web_control_command out;
    bool ok = web_control_command_from_json(json, strlen(json), &out);
    CHECK("plain start decodes with an empty filename (not a failure)"
         , ok && out.command == WEB_CMD_START && out.filename[0] == '\0');

    web_json_free(json);
}

static void
test_file_list_to_json(void)
{
    struct web_file_entry files[2] = {
        { .name = "benchy.gcode", .size_bytes = 6291456, .print_time_s = 8100 },
        { .name = "calibration_cube.gcode", .size_bytes = 838860
          , .print_time_s = 1320 },
    };
    char *json = web_file_list_to_json(files, 2);
    CHECK("file list encodes", json != NULL);
    if (!json)
        return;
    CHECK("file list JSON contains both filenames and a size"
         , strstr(json, "benchy.gcode") != NULL
           && strstr(json, "calibration_cube.gcode") != NULL
           && strstr(json, "6291456") != NULL);
    web_json_free(json);
}

static void
test_file_list_to_json_empty(void)
{
    char *json = web_file_list_to_json(NULL, 0);
    CHECK("an empty file list still encodes to a valid (empty) array"
         , json != NULL && strstr(json, "\"files\":[]") != NULL);
    web_json_free(json);
}

static void
test_camera_config_to_json(void)
{
    struct web_camera_config url_cfg = {.mode = WEB_CAMERA_URL};
    strncpy(url_cfg.url, "http://192.168.1.50/stream", sizeof(url_cfg.url) - 1);
    char *json = web_camera_config_to_json(&url_cfg);
    CHECK("url-mode camera config encodes with its URL"
         , json != NULL && strstr(json, "\"mode\":\"url\"") != NULL
           && strstr(json, "192.168.1.50") != NULL);
    web_json_free(json);

    struct web_camera_config none_cfg = {.mode = WEB_CAMERA_NONE};
    json = web_camera_config_to_json(&none_cfg);
    CHECK("none-mode camera config omits url entirely"
         , json != NULL && strstr(json, "\"mode\":\"none\"") != NULL
           && strstr(json, "url") == NULL);
    web_json_free(json);
}

static void
test_camera_config_roundtrip(void)
{
    struct web_camera_config c = {.mode = WEB_CAMERA_URL};
    strncpy(c.url, "http://192.168.1.50/stream", sizeof(c.url) - 1);
    char *json = web_camera_config_to_json(&c);
    CHECK("url-mode camera config encodes", json != NULL);
    if (!json)
        return;

    struct web_camera_config out;
    bool ok = web_camera_config_from_json(json, strlen(json), &out);
    CHECK("url-mode camera config decodes and roundtrips"
         , ok && out.mode == WEB_CAMERA_URL
           && strcmp(out.url, c.url) == 0);
    web_json_free(json);

    struct web_camera_config local_c = {.mode = WEB_CAMERA_LOCAL};
    json = web_camera_config_to_json(&local_c);
    ok = web_camera_config_from_json(json, strlen(json), &out);
    CHECK("local-mode camera config decodes with no url required"
         , ok && out.mode == WEB_CAMERA_LOCAL);
    web_json_free(json);
}

static void
test_camera_config_rejects_url_mode_without_url(void)
{
    static const char no_url[] = "{\"mode\":\"url\"}";
    struct web_camera_config out;
    CHECK("url-mode camera config decode rejects a missing url"
         , !web_camera_config_from_json(no_url, sizeof(no_url) - 1, &out));

    static const char empty_url[] = "{\"mode\":\"url\",\"url\":\"\"}";
    CHECK("url-mode camera config decode rejects an empty url"
         , !web_camera_config_from_json(empty_url, sizeof(empty_url) - 1, &out));
}

static void
test_camera_config_rejects_unrecognized_mode(void)
{
    static const char bad_mode[] = "{\"mode\":\"drone\"}";
    struct web_camera_config out;
    CHECK("camera config decode rejects an unrecognized mode"
         , !web_camera_config_from_json(bad_mode, sizeof(bad_mode) - 1, &out));
}

static void
test_control_command_gcode_roundtrip(void)
{
    struct web_control_command c = {.command = WEB_CMD_GCODE};
    strncpy(c.gcode, "G28 X", sizeof(c.gcode) - 1);
    char *json = web_control_command_to_json(&c);
    CHECK("gcode command encodes", json != NULL);
    if (!json)
        return;

    struct web_control_command out;
    bool ok = web_control_command_from_json(json, strlen(json), &out);
    CHECK("gcode command decodes and roundtrips its line"
         , ok && out.command == WEB_CMD_GCODE
           && strcmp(out.gcode, "G28 X") == 0);
    web_json_free(json);
}

static void
test_control_command_gcode_rejects_empty_line(void)
{
    static const char empty_gcode[] = "{\"type\":\"command\",\"command\":\"gcode\",\"gcode\":\"\"}";
    struct web_control_command out;
    CHECK("gcode command decode rejects an empty line"
         , !web_control_command_from_json(empty_gcode, sizeof(empty_gcode) - 1, &out));
}

static void
test_control_command_tune_roundtrip(void)
{
    struct web_control_command c = {.command = WEB_CMD_SET_SPEED_FACTOR, .value = 120.0};
    char *json = web_control_command_to_json(&c);
    CHECK("set_speed_factor command encodes", json != NULL);
    if (!json)
        return;

    struct web_control_command out;
    bool ok = web_control_command_from_json(json, strlen(json), &out);
    CHECK("set_speed_factor decodes and roundtrips its value"
         , ok && out.command == WEB_CMD_SET_SPEED_FACTOR
           && dbl_eq(out.value, 120.0));
    web_json_free(json);

    struct web_control_command z = {.command = WEB_CMD_SET_Z_OFFSET, .value = -0.05};
    json = web_control_command_to_json(&z);
    ok = web_control_command_from_json(json, strlen(json), &out);
    CHECK("set_z_offset decodes and roundtrips a negative value"
         , ok && out.command == WEB_CMD_SET_Z_OFFSET && dbl_eq(out.value, -0.05));
    web_json_free(json);
}

static void
test_console_log_roundtrip(void)
{
    struct web_console_log c = {.line = "ok"};
    char *json = web_console_log_to_json(&c);
    CHECK("console_log encodes", json != NULL);
    if (!json)
        return;

    CHECK("web_msg_type_of identifies a console_log message"
         , web_msg_type_of(json, strlen(json)) == WEB_MSG_CONSOLE_LOG);

    struct web_console_log out;
    bool ok = web_console_log_from_json(json, strlen(json), &out);
    CHECK("console_log decodes and roundtrips its line"
         , ok && strcmp(out.line, "ok") == 0);
    web_json_free(json);
}

static void
test_control_command_temp_set_rejects_missing_target(void)
{
    static const char no_target[] =
        "{\"type\":\"command\",\"command\":\"set_bed_temp\"}";
    struct web_control_command out;
    CHECK("set_bed_temp decode rejects a message missing target_temp"
         , !web_control_command_from_json(no_target, sizeof(no_target) - 1
                                          , &out));
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
    test_status_filename_truncation_on_decode();
    test_control_command_start_roundtrip();
    test_control_command_jog_roundtrip();
    test_control_command_filament_roundtrip();
    test_control_command_temp_set_roundtrip();
    test_control_command_temp_set_rejects_missing_target();
    test_control_command_start_with_filename_roundtrip();
    test_control_command_start_without_filename();
    test_file_list_to_json();
    test_file_list_to_json_empty();
    test_camera_config_to_json();
    test_camera_config_roundtrip();
    test_camera_config_rejects_url_mode_without_url();
    test_camera_config_rejects_unrecognized_mode();
    test_control_command_gcode_roundtrip();
    test_control_command_gcode_rejects_empty_line();
    test_control_command_tune_roundtrip();
    test_console_log_roundtrip();
    test_msg_type_of_edge_cases();
    test_decode_rejects_wrong_type();
    test_decode_rejects_unrecognized_enum_value();
    test_decode_rejects_missing_fields();

    printf("\n%s (%d failure%s)\n"
          , g_failures ? "SOME TESTS FAILED" : "ALL TESTS PASSED"
          , g_failures, g_failures == 1 ? "" : "s");
    return g_failures ? 1 : 0;
}
