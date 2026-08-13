// Files screen -- gallery grid + a print-confirm subpage reached by
// tapping a tile, matching the mockup's "TOUCH: FILES GALLERY" and
// "TOUCH: FILE PRINT CONFIRM" panels.
//
// No wire format for file listings exists yet (see ui.c's top comment
// and the mockup's own note: "Files... sketched in but none of them
// have a wire format... behind them yet"), so this is static demo data,
// not read from a real SD card listing. The confirm subpage's "Print"
// button sends SLP_CMD_START -- the closest real command available,
// though slp_control_command has no filename field yet, so it can't
// actually tell main-esp *which* file to print. That gap is real and
// unresolved, not hidden behind a fake success.
#include "ui_internal.h"
#include "ui_theme.h"

struct demo_file {
    const char *name;
    const char *size;
    const char *print_time;
};

static const struct demo_file FILES[] = {
    { "bracket_v3.gcode", "4.2 MB", "1h 48m" },
    { "calibration_cube.gcode", "0.8 MB", "22m" },
    { "benchy.gcode", "6.1 MB", "2h 15m" },
    { "fan_shroud_ptfe.gcode", "3.5 MB", "1h 10m" },
};
#define NUM_FILES (sizeof(FILES) / sizeof(FILES[0]))

static lv_obj_t *g_gallery;
static lv_obj_t *g_detail;
static lv_obj_t *g_detail_name, *g_detail_meta_time, *g_detail_meta_size;
static const struct demo_file *g_detail_file;

static void
send_simple(enum slp_control_cmd cmd)
{
    struct slp_control_command c = {0};
    c.command = cmd;
    ui_send_command(&c);
}

static void
back_to_gallery_cb(lv_event_t *e) { (void)e; ui_show_screen(g_gallery); }

static void
print_cb(lv_event_t *e)
{
    (void)e;
    send_simple(SLP_CMD_START); // see file-header note: no filename field yet
}

static void
open_detail_cb(lv_event_t *e)
{
    const struct demo_file *f = (const struct demo_file *)lv_event_get_user_data(e);
    g_detail_file = f;
    lv_label_set_text(g_detail_name, f->name);
    lv_label_set_text(g_detail_meta_time, f->print_time);
    lv_label_set_text(g_detail_meta_size, f->size);
    ui_show_screen(g_detail);
}

static void
make_gallery_tile(lv_obj_t *parent, int x, int y, int w, int h
                  , const struct demo_file *f)
{
    lv_obj_t *tile = lv_obj_create(parent);
    ui_style_card(tile);
    lv_obj_clear_flag(tile, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(tile, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_size(tile, w, h);
    lv_obj_align(tile, LV_ALIGN_TOP_LEFT, x, y);
    lv_obj_add_event_cb(tile, open_detail_cb, LV_EVENT_CLICKED, (void *)f);

    lv_obj_t *thumb = lv_label_create(tile);
    lv_label_set_text(thumb, LV_SYMBOL_FILE);
    lv_obj_set_style_text_color(thumb, UI_COLOR_TEXT_FAINT, 0);
    lv_obj_align(thumb, LV_ALIGN_TOP_MID, 0, 4);

    lv_obj_t *label = lv_label_create(tile);
    lv_label_set_text(label, f->name);
    lv_obj_set_style_text_color(label, UI_COLOR_INK, 0);
    lv_label_set_long_mode(label, LV_LABEL_LONG_DOT);
    lv_obj_set_width(label, w - 12);
    lv_obj_align(label, LV_ALIGN_BOTTOM_MID, 0, 0);
}

static lv_obj_t *
build_gallery(void)
{
    lv_obj_t *screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screen, UI_COLOR_PAPER, 0);
    lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *pill, *right;
    ui_topbar_create(screen, &pill, &right);
    ui_topbar_set_state(pill, SLP_STATE_IDLE);
    char count_buf[24];
    lv_snprintf(count_buf, sizeof(count_buf), "%u files on SD", (unsigned)NUM_FILES);
    lv_label_set_text(right, count_buf);

    int tile_w = (UI_SCREEN_W - 16 - 2 * 8) / 3;
    int tile_h = (UI_BODY_H - 16 - 8) / 2;
    for (size_t i = 0; i < NUM_FILES; i++) {
        int col = (int)(i % 3), row = (int)(i / 3);
        int x = 8 + col * (tile_w + 8);
        int y = UI_BODY_Y + 8 + row * (tile_h + 8);
        make_gallery_tile(screen, x, y, tile_w, tile_h, &FILES[i]);
    }

    ui_tabbar_create(screen, UI_TAB_FILES);
    return screen;
}

static lv_obj_t *
build_detail(void)
{
    lv_obj_t *screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screen, UI_COLOR_PAPER, 0);
    lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);

    // Custom top strip (back-link + size) -- doesn't fit the state-pill
    // shape ui_topbar_create builds, so this one's hand-rolled rather
    // than forcing that helper to grow a second visual mode.
    lv_obj_t *bar = lv_obj_create(screen);
    lv_obj_remove_style_all(bar);
    lv_obj_set_size(bar, UI_SCREEN_W, UI_TOPBAR_H);
    lv_obj_align(bar, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_set_style_bg_color(bar, UI_COLOR_PANEL, 0);
    lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(bar, UI_COLOR_LINE, 0);
    lv_obj_set_style_border_width(bar, 1, 0);
    lv_obj_set_style_border_side(bar, LV_BORDER_SIDE_BOTTOM, 0);
    lv_obj_clear_flag(bar, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *back = lv_label_create(bar);
    lv_label_set_text(back, LV_SYMBOL_LEFT " Files");
    lv_obj_set_style_text_color(back, UI_COLOR_TEXT_DIM, 0);
    lv_obj_add_flag(back, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(back, back_to_gallery_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_align(back, LV_ALIGN_LEFT_MID, 8, 0);

    lv_obj_t *size_lbl = lv_label_create(bar);
    lv_label_set_text(size_lbl, "--");
    lv_obj_set_style_text_color(size_lbl, UI_COLOR_TEXT_DIM, 0);
    lv_obj_align(size_lbl, LV_ALIGN_RIGHT_MID, -8, 0);
    g_detail_meta_size = size_lbl;

    lv_obj_t *thumb = lv_obj_create(screen);
    ui_style_card(thumb);
    lv_obj_clear_flag(thumb, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(thumb, 160, UI_BODY_H - 16);
    lv_obj_align(thumb, LV_ALIGN_TOP_LEFT, 8, UI_BODY_Y + 8);
    lv_obj_t *thumb_glyph = lv_label_create(thumb);
    lv_label_set_text(thumb_glyph, LV_SYMBOL_FILE);
    lv_obj_set_style_text_font(thumb_glyph, UI_FONT_HERO, 0);
    lv_obj_set_style_text_color(thumb_glyph, UI_COLOR_TEXT_FAINT, 0);
    lv_obj_center(thumb_glyph);

    int info_x = 8 + 160 + 12;
    int info_w = UI_SCREEN_W - info_x - 8;

    g_detail_name = lv_label_create(screen);
    lv_label_set_text(g_detail_name, "--");
    lv_obj_set_style_text_font(g_detail_name, UI_FONT_LABEL, 0);
    lv_obj_set_width(g_detail_name, info_w);
    lv_label_set_long_mode(g_detail_name, LV_LABEL_LONG_WRAP);
    lv_obj_align(g_detail_name, LV_ALIGN_TOP_LEFT, info_x, UI_BODY_Y + 8);

    lv_obj_t *time_label = lv_label_create(screen);
    lv_label_set_text(time_label, "Est. print time");
    lv_obj_set_style_text_color(time_label, UI_COLOR_TEXT_FAINT, 0);
    lv_obj_align(time_label, LV_ALIGN_TOP_LEFT, info_x, UI_BODY_Y + 40);
    g_detail_meta_time = lv_label_create(screen);
    lv_label_set_text(g_detail_meta_time, "--");
    lv_obj_set_style_text_font(g_detail_meta_time, UI_FONT_VALUE, 0);
    lv_obj_align(g_detail_meta_time, LV_ALIGN_TOP_LEFT, info_x, UI_BODY_Y + 54);

    int btn_y = UI_BODY_Y + UI_BODY_H - 44;
    int btn_w = (info_w - 8) / 2;

    lv_obj_t *cancel_btn = lv_btn_create(screen);
    lv_obj_set_size(cancel_btn, btn_w, 40);
    lv_obj_align(cancel_btn, LV_ALIGN_TOP_LEFT, info_x, btn_y);
    lv_obj_set_style_bg_color(cancel_btn, UI_COLOR_PANEL_2, 0);
    lv_obj_add_event_cb(cancel_btn, back_to_gallery_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *cancel_lbl = lv_label_create(cancel_btn);
    lv_label_set_text(cancel_lbl, "Cancel");
    lv_obj_center(cancel_lbl);

    lv_obj_t *print_btn = lv_btn_create(screen);
    lv_obj_set_size(print_btn, btn_w, 40);
    lv_obj_align(print_btn, LV_ALIGN_TOP_LEFT, info_x + btn_w + 8, btn_y);
    lv_obj_set_style_bg_color(print_btn, UI_COLOR_ACCENT, 0);
    lv_obj_add_event_cb(print_btn, print_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *print_lbl = lv_label_create(print_btn);
    lv_label_set_text(print_lbl, LV_SYMBOL_PLAY " Print");
    lv_obj_set_style_text_color(print_lbl, UI_COLOR_ACCENT_INK, 0);
    lv_obj_center(print_lbl);

    ui_tabbar_create(screen, UI_TAB_FILES);
    return screen;
}

lv_obj_t *
screen_files_create(void)
{
    g_detail = build_detail();
    g_gallery = build_gallery();
    return g_gallery;
}
