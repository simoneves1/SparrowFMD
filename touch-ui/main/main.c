// Scaffold entry point -- touch-ui has no real application logic yet
// (see touch-ui/README.md's "Contents (planned)" list). Runs the
// shared-protocol encode/decode self-test on every boot, the same way
// main-esp/main/main.c does for each of its modules, plus a real
// hardware bring-up test of display-driver against the physical 4.0inch
// ESP32-32E board (see PINOUT.md for pin numbers) -- unlike main-esp's
// khp_uart_transport_init/storage_sd_mount/web_server_start, this one
// *is* being run against real hardware as it's brought up, since that
// board is what's on hand right now.
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "slp_frame.h"
#include "slp_messages.h"
#include "display_driver.h"
#include "ui.h"

static const char *TAG = "touch-ui";

// Landscape orientation (480x320) confirmed correct by photo -- red/
// green/blue/yellow corners all landed where the code expects. No longer
// holding on the diagnostic screen.
#define TOUCH_UI_HOLD_ON_ORIENTATION_TEST 0

#define DISPLAY_SPI_HOST SPI2_HOST
#define DISPLAY_PIN_SCLK 14
#define DISPLAY_PIN_MOSI 13
#define DISPLAY_PIN_MISO 12
#define DISPLAY_PIN_LCD_CS 15
#define DISPLAY_PIN_LCD_DC 2
#define DISPLAY_PIN_LCD_BL 27
#define DISPLAY_PIN_TOUCH_CS 33
#define DISPLAY_PIN_TOUCH_IRQ 36
// Landscape, per the operator-UI mockup's design (4-tab bottom nav,
// two-column layouts) -- not the portrait orientation this board was
// originally bring-up-tested in. Same physical panel, same swap_xy/
// mirror_x transform in display_driver.c (that's a fixed hardware
// property of how this glass is bonded, independent of which axis we
// call "width"); only h_res/v_res swap here. See PINOUT.md.
#define DISPLAY_H_RES 480
#define DISPLAY_V_RES 320

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

// First real-hardware test of display-driver: bring up the panel and
// touch controller, then fill the screen solid red one row at a time
// (avoids needing a ~300KB full-frame buffer on plain ESP32's limited
// RAM) so a human looking at the board can confirm the panel is alive
// and note whether colors/orientation come out wrong (see
// display_driver.h's note on mirror/swap/RGB-vs-BGR being unconfirmed).
// Also does one touch read to confirm SPI communication with the
// XPT2046 succeeds, independent of whether the screen is actually
// touched at boot. Hands the resulting handles back via *out_handles
// so app_main can feed them to ui_init() afterwards -- display_driver_init()
// is only called once, here, rather than a second time for the UI.
static bool
hw_test_display(struct display_driver_handles *out_handles)
{
    struct display_driver_config cfg = {
        .spi_host = DISPLAY_SPI_HOST,
        .pin_sclk = DISPLAY_PIN_SCLK, .pin_mosi = DISPLAY_PIN_MOSI
        , .pin_miso = DISPLAY_PIN_MISO,
        .pin_lcd_cs = DISPLAY_PIN_LCD_CS, .pin_lcd_dc = DISPLAY_PIN_LCD_DC
        , .pin_lcd_bl = DISPLAY_PIN_LCD_BL,
        .pin_touch_cs = DISPLAY_PIN_TOUCH_CS, .pin_touch_irq = DISPLAY_PIN_TOUCH_IRQ,
        .h_res = DISPLAY_H_RES, .v_res = DISPLAY_V_RES,
    };
    if (!display_driver_init(out_handles, &cfg)) {
        ESP_LOGE(TAG, "display_driver_init failed");
        return false;
    }
    struct display_driver_handles *h = out_handles;

    // Orientation diagnostic: a solid-color fill can't reveal a
    // rotation/mirror bug (a rotated solid fill still looks like a solid
    // fill -- this is exactly how the earlier "screen filled red" test
    // missed the real-content rotation bug found once the ui component's
    // asymmetric layout exposed it). Four distinct corner colors let a
    // human read back the exact orientation from one glance instead of
    // guessing swap_xy/mirror combinations blind.
    uint16_t black_row[DISPLAY_H_RES];
    for (int i = 0; i < DISPLAY_H_RES; i++)
        black_row[i] = 0x0000;
    for (int y = 0; y < DISPLAY_V_RES; y++) {
        if (esp_lcd_panel_draw_bitmap(h->panel, 0, y, DISPLAY_H_RES, y + 1, black_row) != ESP_OK) {
            ESP_LOGE(TAG, "draw_bitmap failed at row %d", y);
            return false;
        }
    }

    const int box = 40;
    uint16_t block[40];
    struct { int x, y; uint16_t color; const char *name; } corners[] = {
        { 0, 0, 0xF800, "RED" },                                   // top-left
        { DISPLAY_H_RES - box, 0, 0x07E0, "GREEN" },               // top-right
        { 0, DISPLAY_V_RES - box, 0x001F, "BLUE" },                // bottom-left
        { DISPLAY_H_RES - box, DISPLAY_V_RES - box, 0xFFE0, "YELLOW" }, // bottom-right
    };
    for (size_t c = 0; c < sizeof(corners) / sizeof(corners[0]); c++) {
        for (int i = 0; i < box; i++)
            block[i] = corners[c].color;
        for (int y = 0; y < box; y++) {
            if (esp_lcd_panel_draw_bitmap(h->panel, corners[c].x, corners[c].y + y
                                          , corners[c].x + box, corners[c].y + y + 1
                                          , block) != ESP_OK) {
                ESP_LOGE(TAG, "corner draw failed: %s", corners[c].name);
                return false;
            }
        }
    }
    ESP_LOGI(TAG, "orientation test drawn -- RED=logical top-left, "
                  "GREEN=logical top-right, BLUE=logical bottom-left, "
                  "YELLOW=logical bottom-right. Report which color is in "
                  "which physical corner when the board is held landscape "
                  "(long edge horizontal).");

    if (esp_lcd_touch_read_data(h->touch) != ESP_OK) {
        ESP_LOGE(TAG, "touch read_data failed");
        return false;
    }
    ESP_LOGI(TAG, "touch controller responded to SPI read");
    return true;
}

// ui's send-command callback: there's no real transport wired to
// main-esp yet (see PINOUT.md's open UART pin question), so this just
// encodes and logs the command, the same way self_test_shared_protocol()
// exercises encode/decode without a real link. A real
// slp_uart_transport-backed sender plugs into this same seam later.
static void
demo_send_command(const struct slp_control_command *cmd, void *ctx)
{
    (void)ctx;
    uint8_t payload[SLP_CONTROL_COMMAND_WIRE_SIZE];
    slp_control_command_encode(payload, cmd);
    ESP_LOGI(TAG, "ui -> control_command cmd=%d target_temp_x100=%d"
                  " jog_dz_x100=%d feedrate=%u (encoded, not sent -- no"
                  " transport wired yet)"
            , cmd->command, cmd->target_temp_c_x100, cmd->jog_dz_mm_x100
            , (unsigned)cmd->jog_feedrate_mm_min);
}

void
app_main(void)
{
    bool ok = self_test_shared_protocol();
    ESP_LOGI(TAG, "shared-protocol self-test: %s", ok ? "PASS" : "FAIL");

    struct display_driver_handles handles;
    ok = hw_test_display(&handles);
    ESP_LOGI(TAG, "display-driver hw test: %s", ok ? "PASS" : "FAIL");
    if (!ok)
        return;

#if TOUCH_UI_HOLD_ON_ORIENTATION_TEST
    ESP_LOGI(TAG, "holding on the orientation test screen -- ui_init() not"
                  " called, see TOUCH_UI_HOLD_ON_ORIENTATION_TEST");
    for (;;)
        vTaskDelay(pdMS_TO_TICKS(1000));
#endif

    struct ui_config ui_cfg = {
        .display = handles,
        .h_res = DISPLAY_H_RES, .v_res = DISPLAY_V_RES,
        .send_command = demo_send_command, .send_command_ctx = NULL,
    };
    if (!ui_init(&ui_cfg)) {
        ESP_LOGE(TAG, "ui_init failed");
        return;
    }

    // Canned status_update standing in for a real UART receive loop
    // (unwired, see above) -- gives the print-status/temperature
    // screens real content to check against the physical panel.
    struct slp_status_update demo = {
        .state = SLP_STATE_PRINTING,
        .hotend_temp_c_x100 = 20950, .hotend_target_c_x100 = 21000,
        .bed_temp_c_x100 = 5980, .bed_target_c_x100 = 6000,
        .progress_percent = 42, .elapsed_s = 1830,
        .filename = "benchy.gcode",
        .layer_current = 87, .layer_total = 214,
        .remaining_s = 2280,
    };
    ui_status_update(&demo);
    ESP_LOGI(TAG, "ui: demo status_update pushed -- check the panel");
}
