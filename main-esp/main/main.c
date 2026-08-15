// Scaffold entry point. main-esp has no real application logic yet --
// this just self-tests the klipper-host-protocol module on boot so a
// flashed board gives some signal it's alive and the module works on
// real hardware, not just the host-native unit tests. Replace this once
// gcode-parser/kinematics/etc. exist and there's a real app to run.
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "esp_timer.h"
#include "khp_vlq.h"
#include "khp_msgblock.h"
#include "gcode_parser.h"
#include "slp_frame.h"
#include "slp_messages.h"
#include "link_watchdog.h"
#include "cfg_parser.h"
#include "web_api.h"
#include "web_server.h"

static const char *TAG = "main-esp";

static bool
self_test_protocol(void)
{
    uint8_t content[3] = {0x01, 0x2a, 0xff};
    uint8_t buf[KHP_MSG_MAX];
    size_t block_len = khp_msgblock_encode(buf, content, sizeof(content), 5);
    if (block_len == 0)
        return false;

    struct khp_msgblock_scanner scanner = {0};
    int r = khp_msgblock_check(&scanner, buf, (int)block_len);
    if (r != (int)block_len)
        return false;

    struct khp_msgblock_view view;
    khp_msgblock_view_init(&view, buf, r);
    if (view.content_len != sizeof(content) || view.seq != 5)
        return false;
    for (size_t i = 0; i < sizeof(content); i++)
        if (view.content[i] != content[i])
            return false;

    int32_t v = -12345;
    uint8_t vlq_buf[KHP_VLQ_MAX_BYTES];
    khp_vlq_encode_int32(vlq_buf, v);
    const uint8_t *p = vlq_buf;
    return khp_vlq_decode_int32(&p) == v;
}

static bool
self_test_gcode(void)
{
    static const char line[] = "G1 X10.5 Y-20 F3000";
    struct gcode_command cmd;
    enum gcode_status st = gcode_parse_line(line, strlen(line), &cmd);
    if (st != GCODE_OK || cmd.letter != 'G' || cmd.code != 1
        || cmd.param_count != 3)
        return false;
    const struct gcode_param *x = gcode_find_param(&cmd, 'X');
    return x != NULL && x->value == 10.5;
}

static bool
self_test_shared_protocol(void)
{
    struct slp_status_update s = {
        .state = SLP_STATE_PRINTING, .hotend_temp_c_x100 = 21050
        , .hotend_target_c_x100 = 21000, .bed_temp_c_x100 = 6000
        , .bed_target_c_x100 = 6000, .progress_percent = 50
        , .elapsed_s = 120,
    };
    uint8_t payload[SLP_STATUS_UPDATE_WIRE_SIZE];
    slp_status_update_encode(payload, &s);

    uint8_t frame[SLP_FRAME_MAX];
    size_t frame_len = slp_frame_encode(frame, SLP_MSG_STATUS_UPDATE, payload
                                        , sizeof(payload));

    struct slp_frame_scanner scanner = {0};
    int r = slp_frame_check(&scanner, frame, (int)frame_len);
    if (r != (int)frame_len)
        return false;

    struct slp_frame_view view;
    if (slp_frame_view_init(&view, frame, r) != SLP_FRAME_OK)
        return false;

    struct slp_status_update out;
    return slp_status_update_decode(view.payload, view.payload_len, &out)
        && out.progress_percent == 50;
}

static bool
self_test_safety(void)
{
    struct link_watchdog wd;
    link_watchdog_init(&wd, 1000, NULL, NULL);
    link_watchdog_feed(&wd, 0);
    if (link_watchdog_check(&wd, 500) != LINK_OK)
        return false;
    if (link_watchdog_check(&wd, 1000) != LINK_FAULTED)
        return false;
    link_watchdog_reset(&wd, 2000);
    return link_watchdog_check(&wd, 2000) == LINK_OK;
}

static bool
self_test_storage(void)
{
    static const char text[] =
        "[extruder]\n"
        "max_temp: 260\n"
        "rotation_distance = 22.6789\n";
    struct cfg_file cfg;
    if (cfg_parse(text, strlen(text), &cfg) != CFG_OK)
        return false;
    const struct cfg_section *sec = cfg_find_section(&cfg, "extruder");
    long max_temp;
    if (!sec || !cfg_get_long(sec, "max_temp", &max_temp) || max_temp != 260)
        return false;
    // storage_sd_mount() is deliberately not exercised here -- it needs
    // a real SD card/SDMMC peripheral to do anything meaningful, unlike
    // this parser. See storage_sd.h's own "unverified against hardware"
    // note.
    return true;
}

static bool
self_test_web_ui(void)
{
    struct web_status s = {
        .state = WEB_STATE_PRINTING, .hotend_temp = 210.5
        , .hotend_target = 210.0, .bed_temp = 60.0, .bed_target = 60.0
        , .progress_percent = 50, .elapsed_s = 120,
    };
    char *json = web_status_to_json(&s);
    if (!json)
        return false;

    struct web_status out;
    bool ok = web_msg_type_of(json, strlen(json)) == WEB_MSG_STATUS
        && web_status_from_json(json, strlen(json), &out)
        && out.progress_percent == 50;
    web_json_free(json);
    // web_server_start() itself isn't exercised in this JSON-only
    // self-test (a real client wouldn't add anything a self-test can
    // check) -- but unlike before, it IS now actually called for real in
    // app_main() below, not left permanently uncalled. See web_server.h's
    // own "unverified against hardware" note: it cross-compiles and this
    // makes it actually run on boot, but nothing has confirmed a real
    // browser can reach it yet.
    return ok;
}

// -- web-ui callback wiring --------------------------------------------
// main-esp's first real (if minimal) application logic: gives web_server
// something to actually call instead of just existing as an unused
// library. None of this is backed by real persistent storage or a real
// gcode/kinematics execution pipeline yet (neither exists in this repo
// yet) -- each callback below says plainly what it actually does instead
// of silently pretending to be more complete than it is.

// In-RAM only settings text -- lost on reboot. Real persistence is
// storage_sd's job (SD card mount), which isn't wired here; this at
// least exercises cfg_parser's real validation on real hardware, which
// self_test_storage above only does against a fixed test string.
static char s_settings_text[WEB_SERVER_MAX_SETTINGS_BYTES] = "";

// In-RAM only camera config -- lost on reboot, same caveat as settings.
static struct web_camera_config s_camera_config = {.mode = WEB_CAMERA_NONE};

static void
on_web_command(const struct web_control_command *cmd, void *ctx)
{
    (void)ctx;
    // No gcode/kinematics pipeline exists yet to actually act on this --
    // logging what was received is the honest thing to do here, the same
    // "encoded, not sent/executed -- no pipeline wired yet" pattern
    // touch-ui's demo_send_command uses for the same reason.
    ESP_LOGI(TAG, "web command received (not executed -- no gcode/kinematics"
                  " pipeline wired yet): cmd=%d gcode=\"%s\" filename=\"%s\""
                  " value=%.3f target_temp=%.3f jog=(%.2f,%.2f,%.2f)@%.1f"
            , cmd->command, cmd->gcode, cmd->filename, cmd->value
            , cmd->target_temp, cmd->jog_dx, cmd->jog_dy, cmd->jog_dz
            , cmd->jog_feedrate);
}

static size_t
on_web_files_list(struct web_file_entry *out, size_t max, void *ctx)
{
    (void)out; (void)max; (void)ctx;
    // Honestly empty rather than faking demo entries -- no SD card
    // listing exists yet (that's storage_sd's job), see web_server.h.
    return 0;
}

static void
on_web_camera_config(struct web_camera_config *out, void *ctx)
{
    (void)ctx;
    *out = s_camera_config;
}

static bool
on_web_camera_config_set(const struct web_camera_config *cfg, void *ctx)
{
    (void)ctx;
    s_camera_config = *cfg;
    return true;
}

static bool
on_web_settings_get(char *buf, size_t buf_size, void *ctx)
{
    (void)ctx;
    strncpy(buf, s_settings_text, buf_size - 1);
    buf[buf_size - 1] = '\0';
    return true;
}

static bool
on_web_settings_set(const char *text, size_t len, void *ctx)
{
    (void)ctx;
    // Real validation, not a rubber stamp: reject text that doesn't
    // actually parse as printer.cfg-style config before "saving" it,
    // same parser self_test_storage already confirms works.
    struct cfg_file parsed;
    if (cfg_parse(text, len, &parsed) != CFG_OK)
        return false;
    if (len >= sizeof(s_settings_text))
        return false;
    memcpy(s_settings_text, text, len);
    s_settings_text[len] = '\0';
    return true;
}

// Nothing produces real status yet (no gcode/kinematics pipeline), so
// this timer just re-broadcasts one canned "printing" status every 5s --
// enough for a browser connecting at any point after boot to see live
// WebSocket traffic, mirroring touch-ui main.c's one-shot canned push but
// periodic, since web clients (unlike touch-ui's screen) can connect at
// any time after boot, not just at the one moment main.c chooses to push.
static httpd_handle_t s_web_server;

static void
broadcast_demo_status(void *arg)
{
    (void)arg;
    struct web_status s = {
        .state = WEB_STATE_PRINTING, .hotend_temp = 209.5
        , .hotend_target = 210.0, .bed_temp = 59.8, .bed_target = 60.0
        , .progress_percent = 42, .elapsed_s = 1830
        , .filename = "benchy.gcode", .layer_current = 87, .layer_total = 214
        , .remaining_s = 2280, .speed_factor = 100.0, .flow_factor = 100.0
        , .z_offset = 0.0,
    };
    web_server_broadcast_status(s_web_server, &s);
}

static bool
start_web_server(void)
{
    // Minimal TCP/IP stack bring-up -- required for esp_http_server's
    // sockets to work at all, independent of whether any network driver
    // (WiFi/Ethernet) is attached. No driver is brought up here: nothing
    // in this project has picked/wired one yet, so the server starts and
    // binds but isn't reachable from anywhere until that separate,
    // still-open piece of work happens.
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    struct web_server_config cfg = {
        .port = 80, .on_command = on_web_command
        , .on_files_list = on_web_files_list
        , .on_camera_config = on_web_camera_config
        , .on_camera_config_set = on_web_camera_config_set
        , .on_settings_get = on_web_settings_get
        , .on_settings_set = on_web_settings_set,
    };
    s_web_server = web_server_start(&cfg);
    if (!s_web_server)
        return false;

    const esp_timer_create_args_t timer_args = {
        .callback = broadcast_demo_status, .name = "web_status_demo",
    };
    esp_timer_handle_t timer;
    if (esp_timer_create(&timer_args, &timer) != ESP_OK)
        return false;
    esp_timer_start_periodic(timer, 5000000); // 5s, in microseconds
    return true;
}

void
app_main(void)
{
    bool ok = self_test_protocol();
    ESP_LOGI(TAG, "klipper-host-protocol self-test: %s", ok ? "PASS" : "FAIL");

    ok = self_test_gcode();
    ESP_LOGI(TAG, "gcode-parser self-test: %s", ok ? "PASS" : "FAIL");

    ok = self_test_shared_protocol();
    ESP_LOGI(TAG, "shared-protocol self-test: %s", ok ? "PASS" : "FAIL");

    ok = self_test_safety();
    ESP_LOGI(TAG, "safety self-test: %s", ok ? "PASS" : "FAIL");

    ok = self_test_storage();
    ESP_LOGI(TAG, "storage self-test: %s", ok ? "PASS" : "FAIL");

    ok = self_test_web_ui();
    ESP_LOGI(TAG, "web-ui self-test: %s", ok ? "PASS" : "FAIL");

    ok = start_web_server();
    ESP_LOGI(TAG, "web-ui server start: %s (port 80 -- unreachable until a"
                  " network driver is wired up, see start_web_server()'s"
                  " comment)", ok ? "PASS" : "FAIL");
}
