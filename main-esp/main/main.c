// Scaffold entry point. main-esp has no real application logic yet --
// this just self-tests the klipper-host-protocol module on boot so a
// flashed board gives some signal it's alive and the module works on
// real hardware, not just the host-native unit tests. Replace this once
// gcode-parser/kinematics/etc. exist and there's a real app to run.
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include "esp_log.h"
#include "khp_vlq.h"
#include "khp_msgblock.h"
#include "gcode_parser.h"
#include "slp_frame.h"
#include "slp_messages.h"
#include "link_watchdog.h"

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
}
