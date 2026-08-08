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

void
app_main(void)
{
    bool ok = self_test_protocol();
    ESP_LOGI(TAG, "klipper-host-protocol self-test: %s", ok ? "PASS" : "FAIL");

    ok = self_test_gcode();
    ESP_LOGI(TAG, "gcode-parser self-test: %s", ok ? "PASS" : "FAIL");
}
