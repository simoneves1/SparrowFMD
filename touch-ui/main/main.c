// Scaffold entry point -- touch-ui has no real display/input logic yet
// (see touch-ui/README.md's "Contents (planned)" list). This just
// self-tests shared-protocol on boot, the same way main-esp/main/main.c
// does for each of its modules, so a flashed board gives some signal
// it's alive.
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include "esp_log.h"
#include "slp_frame.h"
#include "slp_messages.h"

static const char *TAG = "touch-ui";

// Exercises the same encode/decode path touch-ui will eventually use
// for real: build a control_command (as if the user tapped "jog" on
// the touchscreen) and confirm it frames/parses correctly -- the
// logical inverse of main-esp's self_test_shared_protocol(), which
// builds a status_update instead.
static bool
self_test_shared_protocol(void)
{
    struct slp_control_command c = {
        .command = SLP_CMD_JOG, .jog_dx_mm_x100 = 500 // 5.00mm
        , .jog_dy_mm_x100 = 0, .jog_dz_mm_x100 = 0
        , .jog_feedrate_mm_min = 3000,
    };
    uint8_t payload[SLP_CONTROL_COMMAND_WIRE_SIZE];
    slp_control_command_encode(payload, &c);

    uint8_t frame[SLP_FRAME_MAX];
    size_t frame_len = slp_frame_encode(frame, SLP_MSG_CONTROL_COMMAND, payload
                                        , sizeof(payload));

    struct slp_frame_scanner scanner = {0};
    int r = slp_frame_check(&scanner, frame, (int)frame_len);
    if (r != (int)frame_len)
        return false;

    struct slp_frame_view view;
    if (slp_frame_view_init(&view, frame, r) != SLP_FRAME_OK)
        return false;

    struct slp_control_command out;
    return slp_control_command_decode(view.payload, view.payload_len, &out)
        && out.command == SLP_CMD_JOG && out.jog_dx_mm_x100 == 500;
    // slp_uart_transport_init() is deliberately not exercised here -- it
    // needs a real UART peripheral and a real main-esp on the other end
    // to do anything meaningful, unlike this encode/decode. See
    // slp_uart_transport.h's own "unverified against hardware" note.
}

void
app_main(void)
{
    bool ok = self_test_shared_protocol();
    ESP_LOGI(TAG, "shared-protocol self-test: %s", ok ? "PASS" : "FAIL");
}
