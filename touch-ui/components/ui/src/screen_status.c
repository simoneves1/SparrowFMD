// Status screen -- landscape two-column layout matching the operator-UI
// mockup's "TOUCH: STATUS" panel: left column is hotend/bed temp cards,
// right column is the progress card plus Pause/Stop. Replaces the old
// portrait screen_print_status.c's single-column layout (see
// touch-ui/README.md for the mockup's context).
#include <stdio.h>
#include <stdlib.h>
#include "ui_internal.h"
#include "ui_theme.h"

#define COL_L_X 8
#define COL_L_W 224
#define COL_R_X 240
#define COL_R_W 232

static lv_obj_t *g_screen;
static lv_obj_t *g_state_pill;
static lv_obj_t *g_elapsed_label;

static lv_obj_t *g_hotend_value, *g_hotend_target, *g_hotend_bar;
static lv_obj_t *g_bed_value, *g_bed_target, *g_bed_bar;
static lv_obj_t *g_progress_pct, *g_progress_bar;

static lv_obj_t *g_pause_btn, *g_resume_btn;
static enum slp_print_state g_last_state = SLP_STATE_IDLE;

static void
send_simple(enum slp_control_cmd cmd)
{
    struct slp_control_command c = {0};
    c.command = cmd;
    ui_send_command(&c);
}

static void
pause_cb(lv_event_t *e) { (void)e; send_simple(SLP_CMD_PAUSE); }
static void
resume_cb(lv_event_t *e) { (void)e; send_simple(SLP_CMD_RESUME); }
static void
stop_cb(lv_event_t *e) { (void)e; send_simple(SLP_CMD_STOP); }

// One temp card: label, big value (set later via *out_value), target
// line, and a thin heat-bar. Returns the value/target/bar handles so
// screen_status_set() can update them.
static void
make_temp_card(lv_obj_t *parent, int y, int h, const char *name
              , lv_obj_t **out_value, lv_obj_t **out_target
              , lv_obj_t **out_bar)
{
    lv_obj_t *card = lv_obj_create(parent);
    ui_style_card(card);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(card, COL_L_W, h);
    lv_obj_align(card, LV_ALIGN_TOP_LEFT, 0, y);

    lv_obj_t *label = lv_label_create(card);
    lv_label_set_text(label, name);
    lv_obj_set_style_text_color(label, UI_COLOR_TEXT_FAINT, 0);
    lv_obj_align(label, LV_ALIGN_TOP_LEFT, 0, 0);

    lv_obj_t *value = lv_label_create(card);
    lv_label_set_text(value, "--.-C");
    lv_obj_set_style_text_color(value, UI_COLOR_EMBER, 0);
    lv_obj_set_style_text_font(value, UI_FONT_VALUE, 0);
    lv_obj_align(value, LV_ALIGN_TOP_LEFT, 0, 16);

    lv_obj_t *target = lv_label_create(card);
    lv_label_set_text(target, "-> --.-C");
    lv_obj_set_style_text_color(target, UI_COLOR_TEXT_FAINT, 0);
    lv_obj_align(target, LV_ALIGN_TOP_LEFT, 0, 42);

    lv_obj_t *bar = lv_bar_create(card);
    lv_obj_set_size(bar, COL_L_W - 16, 4);
    lv_obj_align(bar, LV_ALIGN_BOTTOM_LEFT, 0, 0);
    lv_obj_set_style_bg_color(bar, UI_COLOR_PAPER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(bar, UI_COLOR_EMBER, LV_PART_INDICATOR);
    lv_bar_set_range(bar, 0, 100);
    lv_bar_set_value(bar, 0, LV_ANIM_OFF);

    *out_value = value;
    *out_target = target;
    *out_bar = bar;
}

lv_obj_t *
screen_status_create(void)
{
    g_screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(g_screen, UI_COLOR_PAPER, 0);
    lv_obj_clear_flag(g_screen, LV_OBJ_FLAG_SCROLLABLE);

    ui_topbar_create(g_screen, &g_state_pill, &g_elapsed_label);

    int card_h = (UI_BODY_H - 8) / 2;
    make_temp_card(g_screen, UI_BODY_Y + 6, card_h, "Hotend"
                  , &g_hotend_value, &g_hotend_target, &g_hotend_bar);
    make_temp_card(g_screen, UI_BODY_Y + 6 + card_h + 8, card_h, "Bed"
                  , &g_bed_value, &g_bed_target, &g_bed_bar);

    // Progress card, right column, top half.
    int prog_h = UI_BODY_H - 8 - 52;
    lv_obj_t *prog_card = lv_obj_create(g_screen);
    ui_style_card(prog_card);
    lv_obj_clear_flag(prog_card, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(prog_card, COL_R_W, prog_h);
    lv_obj_align(prog_card, LV_ALIGN_TOP_LEFT, COL_R_X, UI_BODY_Y + 6);

    g_progress_pct = lv_label_create(prog_card);
    lv_label_set_text(g_progress_pct, "0%");
    lv_obj_set_style_text_font(g_progress_pct, UI_FONT_HERO, 0);
    lv_obj_align(g_progress_pct, LV_ALIGN_TOP_LEFT, 0, 0);

    lv_obj_t *caption = lv_label_create(prog_card);
    lv_label_set_text(caption, "complete");
    lv_obj_set_style_text_color(caption, UI_COLOR_TEXT_FAINT, 0);
    lv_obj_align(caption, LV_ALIGN_TOP_RIGHT, 0, 6);

    g_progress_bar = lv_bar_create(prog_card);
    lv_obj_set_size(g_progress_bar, COL_R_W - 16, 8);
    lv_obj_align(g_progress_bar, LV_ALIGN_BOTTOM_LEFT, 0, 0);
    lv_obj_set_style_bg_color(g_progress_bar, UI_COLOR_PAPER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(g_progress_bar, UI_COLOR_ACCENT, LV_PART_INDICATOR);
    lv_bar_set_range(g_progress_bar, 0, 100);
    lv_bar_set_value(g_progress_bar, 0, LV_ANIM_OFF);

    // Pause/Stop row, right column, bottom.
    int btn_y = UI_BODY_Y + UI_BODY_H - 44;
    int btn_w = (COL_R_W - 8) / 2;

    g_pause_btn = lv_btn_create(g_screen);
    lv_obj_set_size(g_pause_btn, btn_w, 40);
    lv_obj_align(g_pause_btn, LV_ALIGN_TOP_LEFT, COL_R_X, btn_y);
    lv_obj_set_style_bg_color(g_pause_btn, UI_COLOR_PANEL_2, 0);
    lv_obj_add_event_cb(g_pause_btn, pause_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *pause_lbl = lv_label_create(g_pause_btn);
    lv_label_set_text(pause_lbl, LV_SYMBOL_PAUSE " Pause");
    lv_obj_center(pause_lbl);

    g_resume_btn = lv_btn_create(g_screen);
    lv_obj_set_size(g_resume_btn, btn_w, 40);
    lv_obj_align(g_resume_btn, LV_ALIGN_TOP_LEFT, COL_R_X, btn_y);
    lv_obj_set_style_bg_color(g_resume_btn, UI_COLOR_ACCENT, 0);
    lv_obj_add_event_cb(g_resume_btn, resume_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *resume_lbl = lv_label_create(g_resume_btn);
    lv_label_set_text(resume_lbl, LV_SYMBOL_PLAY " Resume");
    lv_obj_set_style_text_color(resume_lbl, UI_COLOR_ACCENT_INK, 0);
    lv_obj_center(resume_lbl);
    lv_obj_add_flag(g_resume_btn, LV_OBJ_FLAG_HIDDEN);

    lv_obj_t *stop_btn = lv_btn_create(g_screen);
    lv_obj_set_size(stop_btn, btn_w, 40);
    lv_obj_align(stop_btn, LV_ALIGN_TOP_LEFT, COL_R_X + btn_w + 8, btn_y);
    lv_obj_set_style_bg_opa(stop_btn, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_color(stop_btn, UI_COLOR_STATE_ERROR, 0);
    lv_obj_set_style_border_width(stop_btn, 1, 0);
    lv_obj_add_event_cb(stop_btn, stop_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *stop_lbl = lv_label_create(stop_btn);
    lv_label_set_text(stop_lbl, LV_SYMBOL_STOP " Stop");
    lv_obj_set_style_text_color(stop_lbl, UI_COLOR_STATE_ERROR, 0);
    lv_obj_center(stop_lbl);

    ui_tabbar_create(g_screen, UI_TAB_STATUS);

    return g_screen;
}

void
screen_status_set(const struct slp_status_update *s)
{
    ui_topbar_set_state(g_state_pill, s->state);

    unsigned mins = s->elapsed_s / 60, secs = s->elapsed_s % 60;
    char buf[24];
    snprintf(buf, sizeof(buf), "%u:%02u", mins, secs);
    lv_label_set_text(g_elapsed_label, buf);

    snprintf(buf, sizeof(buf), "%d.%01dC"
            , s->hotend_temp_c_x100 / 100, abs(s->hotend_temp_c_x100 % 100) / 10);
    lv_label_set_text(g_hotend_value, buf);
    snprintf(buf, sizeof(buf), "-> %d.%01dC"
            , s->hotend_target_c_x100 / 100, abs(s->hotend_target_c_x100 % 100) / 10);
    lv_label_set_text(g_hotend_target, buf);
    lv_bar_set_value(g_hotend_bar, s->hotend_target_c_x100 > 0
                     ? (s->hotend_temp_c_x100 * 100) / s->hotend_target_c_x100 : 0
                     , LV_ANIM_OFF);

    snprintf(buf, sizeof(buf), "%d.%01dC"
            , s->bed_temp_c_x100 / 100, abs(s->bed_temp_c_x100 % 100) / 10);
    lv_label_set_text(g_bed_value, buf);
    snprintf(buf, sizeof(buf), "-> %d.%01dC"
            , s->bed_target_c_x100 / 100, abs(s->bed_target_c_x100 % 100) / 10);
    lv_label_set_text(g_bed_target, buf);
    lv_bar_set_value(g_bed_bar, s->bed_target_c_x100 > 0
                     ? (s->bed_temp_c_x100 * 100) / s->bed_target_c_x100 : 0
                     , LV_ANIM_OFF);

    snprintf(buf, sizeof(buf), "%u%%", (unsigned)s->progress_percent);
    lv_label_set_text(g_progress_pct, buf);
    lv_bar_set_value(g_progress_bar, s->progress_percent, LV_ANIM_OFF);

    if (s->state != g_last_state) {
        bool paused = s->state == SLP_STATE_PAUSED;
        if (paused) {
            lv_obj_add_flag(g_pause_btn, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(g_resume_btn, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_clear_flag(g_pause_btn, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(g_resume_btn, LV_OBJ_FLAG_HIDDEN);
        }
        g_last_state = s->state;
    }
}
