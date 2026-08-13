// Jog screen -- 3x3 XY pad + vertical Z cluster, matching the mockup's
// "TOUCH: JOG" panel. No step-size selector here (the mockup drops it
// for touch -- see its own "step chips removed" note): a fixed step
// keeps this a quick physical control, same philosophy as the old
// screen_filament.c's fixed retract/extrude/load/unload distances.
// slp_control_command has only one SLP_CMD_HOME (no per-axis home), so
// both the XY pad's center button and the Z cluster's home button send
// the same command.
#include "ui_internal.h"
#include "ui_theme.h"

#define JOG_STEP_MM_X100 1000  // 10.00mm
#define JOG_FEEDRATE_XY 3000
#define JOG_FEEDRATE_Z 600

#define COL_L_X 8
#define COL_L_W 224
#define COL_R_X 240
#define COL_R_W 232

static lv_obj_t *g_screen;

static void
send_jog(int16_t dx, int16_t dy, int16_t dz, uint16_t feedrate)
{
    struct slp_control_command c = {0};
    c.command = SLP_CMD_JOG;
    c.jog_dx_mm_x100 = dx;
    c.jog_dy_mm_x100 = dy;
    c.jog_dz_mm_x100 = dz;
    c.jog_feedrate_mm_min = feedrate;
    ui_send_command(&c);
}

static void
send_simple(enum slp_control_cmd cmd)
{
    struct slp_control_command c = {0};
    c.command = cmd;
    ui_send_command(&c);
}

static void jog_up_cb(lv_event_t *e)    { (void)e; send_jog(0, JOG_STEP_MM_X100, 0, JOG_FEEDRATE_XY); }
static void jog_down_cb(lv_event_t *e)  { (void)e; send_jog(0, -JOG_STEP_MM_X100, 0, JOG_FEEDRATE_XY); }
static void jog_left_cb(lv_event_t *e)  { (void)e; send_jog(-JOG_STEP_MM_X100, 0, 0, JOG_FEEDRATE_XY); }
static void jog_right_cb(lv_event_t *e) { (void)e; send_jog(JOG_STEP_MM_X100, 0, 0, JOG_FEEDRATE_XY); }
static void jog_z_up_cb(lv_event_t *e)   { (void)e; send_jog(0, 0, JOG_STEP_MM_X100, JOG_FEEDRATE_Z); }
static void jog_z_down_cb(lv_event_t *e) { (void)e; send_jog(0, 0, -JOG_STEP_MM_X100, JOG_FEEDRATE_Z); }
static void home_cb(lv_event_t *e)   { (void)e; send_simple(SLP_CMD_HOME); }
static void start_cb(lv_event_t *e)  { (void)e; send_simple(SLP_CMD_START); }

static lv_obj_t *
make_pad_btn(lv_obj_t *parent, int col, int row, int cell, const char *text
            , lv_event_cb_t cb, bool is_home)
{
    lv_obj_t *btn = lv_btn_create(parent);
    lv_obj_set_size(btn, cell, cell);
    lv_obj_align(btn, LV_ALIGN_TOP_LEFT, col * (cell + 6), row * (cell + 6));
    lv_obj_set_style_bg_color(btn, is_home ? UI_COLOR_PANEL_2 : UI_COLOR_PANEL, 0);
    lv_obj_set_style_border_color(btn, UI_COLOR_LINE, 0);
    lv_obj_set_style_border_width(btn, 1, 0);
    lv_obj_set_style_radius(btn, 8, 0);
    if (cb)
        lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *lbl = lv_label_create(btn);
    lv_label_set_text(lbl, text);
    if (is_home) {
        lv_obj_set_style_text_color(lbl, UI_COLOR_ACCENT, 0);
        lv_obj_set_style_text_font(lbl, UI_FONT_LABEL, 0);
    }
    lv_obj_center(lbl);
    return btn;
}

lv_obj_t *
screen_jog_create(void)
{
    g_screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(g_screen, UI_COLOR_PAPER, 0);
    lv_obj_clear_flag(g_screen, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *pill, *right;
    ui_topbar_create(g_screen, &pill, &right);
    ui_topbar_set_state(pill, SLP_STATE_IDLE);
    lv_label_set_text(right, "");

    // 3x3 XY pad, left column, roughly square, vertically centered.
    int cell = 48;
    int pad_size = cell * 3 + 12;
    int pad_x = COL_L_X + (COL_L_W - pad_size) / 2;
    int pad_y = UI_BODY_Y + (UI_BODY_H - pad_size) / 2;
    lv_obj_t *pad = lv_obj_create(g_screen);
    lv_obj_remove_style_all(pad);
    lv_obj_set_size(pad, pad_size, pad_size);
    lv_obj_align(pad, LV_ALIGN_TOP_LEFT, pad_x, pad_y);
    lv_obj_clear_flag(pad, LV_OBJ_FLAG_SCROLLABLE);

    make_pad_btn(pad, 1, 0, cell, LV_SYMBOL_UP, jog_up_cb, false);
    make_pad_btn(pad, 0, 1, cell, LV_SYMBOL_LEFT, jog_left_cb, false);
    make_pad_btn(pad, 1, 1, cell, "Home", home_cb, true);
    make_pad_btn(pad, 2, 1, cell, LV_SYMBOL_RIGHT, jog_right_cb, false);
    make_pad_btn(pad, 1, 2, cell, LV_SYMBOL_DOWN, jog_down_cb, false);

    // Z cluster, right column, top: 3 stacked buttons.
    int z_w = 90;
    int z_h = (UI_BODY_H - 8 - 52) - 12;
    int z_btn_h = (z_h - 12) / 3;
    int z_x = COL_R_X + (COL_R_W - z_w) / 2;
    lv_obj_t *z_up = make_pad_btn(g_screen, 0, 0, 0, "", NULL, false);
    lv_obj_set_size(z_up, z_w, z_btn_h);
    lv_obj_align(z_up, LV_ALIGN_TOP_LEFT, z_x, UI_BODY_Y + 6);
    lv_obj_add_event_cb(z_up, jog_z_up_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *z_up_lbl = lv_label_create(z_up);
    lv_label_set_text(z_up_lbl, "Z" LV_SYMBOL_UP);
    lv_obj_center(z_up_lbl);

    lv_obj_t *z_home = make_pad_btn(g_screen, 0, 0, 0, "", NULL, true);
    lv_obj_set_size(z_home, z_w, z_btn_h);
    lv_obj_align(z_home, LV_ALIGN_TOP_LEFT, z_x, UI_BODY_Y + 6 + z_btn_h + 6);
    lv_obj_add_event_cb(z_home, home_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *z_home_lbl = lv_label_create(z_home);
    lv_label_set_text(z_home_lbl, "Z Home");
    lv_obj_set_style_text_color(z_home_lbl, UI_COLOR_ACCENT, 0);
    lv_obj_center(z_home_lbl);

    lv_obj_t *z_down = make_pad_btn(g_screen, 0, 0, 0, "", NULL, false);
    lv_obj_set_size(z_down, z_w, z_btn_h);
    lv_obj_align(z_down, LV_ALIGN_TOP_LEFT, z_x, UI_BODY_Y + 6 + 2 * (z_btn_h + 6));
    lv_obj_add_event_cb(z_down, jog_z_down_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *z_down_lbl = lv_label_create(z_down);
    lv_label_set_text(z_down_lbl, "Z" LV_SYMBOL_DOWN);
    lv_obj_center(z_down_lbl);

    // Home all / Start, right column, bottom.
    int btn_y = UI_BODY_Y + UI_BODY_H - 44;
    int btn_w = (COL_R_W - 8) / 2;

    lv_obj_t *home_all = lv_btn_create(g_screen);
    lv_obj_set_size(home_all, btn_w, 40);
    lv_obj_align(home_all, LV_ALIGN_TOP_LEFT, COL_R_X, btn_y);
    lv_obj_set_style_bg_color(home_all, UI_COLOR_PANEL_2, 0);
    lv_obj_add_event_cb(home_all, home_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *home_all_lbl = lv_label_create(home_all);
    lv_label_set_text(home_all_lbl, LV_SYMBOL_HOME " Home all");
    lv_obj_center(home_all_lbl);

    lv_obj_t *start_btn = lv_btn_create(g_screen);
    lv_obj_set_size(start_btn, btn_w, 40);
    lv_obj_align(start_btn, LV_ALIGN_TOP_LEFT, COL_R_X + btn_w + 8, btn_y);
    lv_obj_set_style_bg_color(start_btn, UI_COLOR_ACCENT, 0);
    lv_obj_add_event_cb(start_btn, start_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *start_lbl = lv_label_create(start_btn);
    lv_label_set_text(start_lbl, LV_SYMBOL_PLAY " Start");
    lv_obj_set_style_text_color(start_lbl, UI_COLOR_ACCENT_INK, 0);
    lv_obj_center(start_lbl);

    ui_tabbar_create(g_screen, UI_TAB_JOG);

    return g_screen;
}
