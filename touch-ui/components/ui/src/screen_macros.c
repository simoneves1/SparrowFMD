// Macros screen -- 6-button grid matching the mockup's "TOUCH: MACROS"
// panel. Four buttons map onto real slp_control_command commands
// (filament load/unload already existed via the old screen_filament.c;
// preheat presets are two SET_*_TEMP sends back to back, same shape as
// screen_temperature.c's old preset buttons). "Bed mesh" has no
// corresponding command in slp_messages.h at all -- there's nothing
// honest to wire it to yet, so its button is present (matching the
// mockup) but intentionally does nothing rather than sending a command
// that doesn't mean what the label says.
#include <string.h>
#include "ui_internal.h"
#include "ui_theme.h"

#define FILAMENT_MOVE_MM_X100 5000   // 50.00mm
#define FILAMENT_FEEDRATE_MM_MIN 300

static void
send_filament(enum slp_control_cmd cmd)
{
    struct slp_control_command c = {0};
    c.command = cmd;
    c.jog_dz_mm_x100 = FILAMENT_MOVE_MM_X100;
    c.jog_feedrate_mm_min = FILAMENT_FEEDRATE_MM_MIN;
    ui_send_command(&c);
}

static void
send_preheat(int16_t hotend_c, int16_t bed_c)
{
    struct slp_control_command c = {0};
    c.command = SLP_CMD_SET_HOTEND_TEMP;
    c.target_temp_c_x100 = (int16_t)(hotend_c * 100);
    ui_send_command(&c);

    memset(&c, 0, sizeof(c));
    c.command = SLP_CMD_SET_BED_TEMP;
    c.target_temp_c_x100 = (int16_t)(bed_c * 100);
    ui_send_command(&c);
}

static void load_cb(lv_event_t *e)     { (void)e; send_filament(SLP_CMD_FILAMENT_LOAD); }
static void unload_cb(lv_event_t *e)   { (void)e; send_filament(SLP_CMD_FILAMENT_UNLOAD); }
static void pla_cb(lv_event_t *e)      { (void)e; send_preheat(200, 60); }
static void petg_cb(lv_event_t *e)     { (void)e; send_preheat(230, 80); }
static void cooldown_cb(lv_event_t *e) { (void)e; send_preheat(0, 0); }
static void bed_mesh_cb(lv_event_t *e) { (void)e; } // see file header note

static void
make_macro_btn(lv_obj_t *parent, int col, int row, int w, int h
              , const char *glyph, const char *label, lv_event_cb_t cb)
{
    lv_obj_t *btn = lv_btn_create(parent);
    lv_obj_set_size(btn, w, h);
    lv_obj_align(btn, LV_ALIGN_TOP_LEFT, 8 + col * (w + 8)
                , UI_BODY_Y + 8 + row * (h + 8));
    lv_obj_set_style_bg_color(btn, UI_COLOR_PANEL_2, 0);
    lv_obj_set_style_border_color(btn, UI_COLOR_LINE, 0);
    lv_obj_set_style_border_width(btn, 1, 0);
    lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *g = lv_label_create(btn);
    lv_label_set_text(g, glyph);
    lv_obj_set_style_text_color(g, UI_COLOR_ACCENT, 0);
    lv_obj_align(g, LV_ALIGN_TOP_MID, 0, 4);

    lv_obj_t *l = lv_label_create(btn);
    lv_label_set_text(l, label);
    lv_obj_align(l, LV_ALIGN_BOTTOM_MID, 0, -4);
}

lv_obj_t *
screen_macros_create(void)
{
    lv_obj_t *screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screen, UI_COLOR_PAPER, 0);
    lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *pill, *right;
    ui_topbar_create(screen, &pill, &right);
    ui_topbar_set_state(pill, SLP_STATE_IDLE);
    lv_label_set_text(right, "");

    int w = (UI_SCREEN_W - 16 - 2 * 8) / 3;
    int h = (UI_BODY_H - 16 - 8) / 2;

    make_macro_btn(screen, 0, 0, w, h, LV_SYMBOL_UPLOAD, "Load filament", load_cb);
    make_macro_btn(screen, 1, 0, w, h, LV_SYMBOL_DOWNLOAD, "Unload filament", unload_cb);
    make_macro_btn(screen, 2, 0, w, h, LV_SYMBOL_CHARGE, "Preheat PLA", pla_cb);
    make_macro_btn(screen, 0, 1, w, h, LV_SYMBOL_CHARGE, "Preheat PETG", petg_cb);
    make_macro_btn(screen, 1, 1, w, h, LV_SYMBOL_LIST, "Bed mesh", bed_mesh_cb);
    make_macro_btn(screen, 2, 1, w, h, LV_SYMBOL_POWER, "Cooldown", cooldown_cb);

    ui_tabbar_create(screen, UI_TAB_MACROS);

    return screen;
}
