// LVGL bring-up + screen wiring. Four tab screens (Status/Jog/Files/
// Macros, landscape 480x320) per the operator-UI mockup -- see ui.h's
// top comment for what's real vs. still a UI-only stub (Files/Macros'
// backend gaps).
#include "ui.h"
#include "ui_internal.h"
#include "esp_lvgl_port.h"
#include "esp_log.h"

static const char *TAG = "ui";

static ui_send_command_fn g_send_command;
static void *g_send_command_ctx;

void
ui_send_command(const struct slp_control_command *cmd)
{
    if (g_send_command)
        g_send_command(cmd, g_send_command_ctx);
}

void
ui_show_screen(lv_obj_t *screen)
{
    lv_scr_load(screen);
}

bool
ui_init(const struct ui_config *cfg)
{
    g_send_command = cfg->send_command;
    g_send_command_ctx = cfg->send_command_ctx;

    const lvgl_port_cfg_t port_cfg = ESP_LVGL_PORT_INIT_CONFIG();
    if (lvgl_port_init(&port_cfg) != ESP_OK) {
        ESP_LOGE(TAG, "lvgl_port_init failed");
        return false;
    }

    // Partial draw buffer (a handful of rows, double-buffered) rather
    // than a full 320x480 frame buffer -- plain ESP32 (no PSRAM on this
    // board, see PINOUT.md) doesn't have room for the latter.
    const lvgl_port_display_cfg_t disp_cfg = {
        .io_handle = cfg->display.lcd_io,
        .panel_handle = cfg->display.panel,
        .buffer_size = (size_t)(cfg->h_res * 40),
        .double_buffer = true,
        .hres = (uint32_t)cfg->h_res,
        .vres = (uint32_t)cfg->v_res,
        .monochrome = false,
        // Must match display_driver.c's init_panel() transform exactly --
        // esp_lvgl_port resets the panel's swap_xy/mirror state to this
        // field's value at init (lvgl_port_update_callback()), silently
        // overwriting whatever display_driver.c set beforehand if this
        // doesn't agree with it. See display_driver.c's comment.
        .rotation = {
            .swap_xy = true,
            .mirror_x = true,
            .mirror_y = true,
        },
        .flags = {
            .buff_dma = true,
        },
    };
    lv_disp_t *disp = lvgl_port_add_disp(&disp_cfg);
    if (disp == NULL) {
        ESP_LOGE(TAG, "lvgl_port_add_disp failed");
        return false;
    }

    const lvgl_port_touch_cfg_t touch_cfg = {
        .disp = disp,
        .handle = cfg->display.touch,
    };
    if (lvgl_port_add_touch(&touch_cfg) == NULL) {
        ESP_LOGE(TAG, "lvgl_port_add_touch failed");
        return false;
    }

    lvgl_port_lock(0);
    lv_obj_t *status = screen_status_create();
    lv_obj_t *jog = screen_jog_create();
    lv_obj_t *files = screen_files_create();
    lv_obj_t *macros = screen_macros_create();

    // Populates ui_chrome's tab registry so each screen's tab bar (built
    // during the screen_*_create() calls above) can navigate to the
    // other 3 by index -- see ui_internal.h's note on why this replaced
    // the old pairwise set_nav chain.
    ui_screens_register(status, jog, files, macros);

    ui_show_screen(status);
    lvgl_port_unlock();

    ESP_LOGI(TAG, "ui_init complete: 4 tab screens built (status/jog/"
                  "files/macros), status shown");
    return true;
}

void
ui_status_update(const struct slp_status_update *s)
{
    lvgl_port_lock(0);
    screen_status_set(s);
    lvgl_port_unlock();
}
