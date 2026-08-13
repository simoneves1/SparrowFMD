#include "ui_chrome.h"
#include "ui_theme.h"
#include "ui_internal.h"

static struct {
    lv_obj_t *status, *jog, *files, *macros;
} g_screens;

void
ui_screens_register(lv_obj_t *status, lv_obj_t *jog
                    , lv_obj_t *files, lv_obj_t *macros)
{
    g_screens.status = status;
    g_screens.jog = jog;
    g_screens.files = files;
    g_screens.macros = macros;
}

lv_obj_t *
ui_screen_for_tab(enum ui_tab tab)
{
    switch (tab) {
    case UI_TAB_STATUS: return g_screens.status;
    case UI_TAB_JOG:    return g_screens.jog;
    case UI_TAB_FILES:  return g_screens.files;
    case UI_TAB_MACROS: return g_screens.macros;
    default:            return NULL;
    }
}

void
ui_topbar_create(lv_obj_t *parent, lv_obj_t **out_pill, lv_obj_t **out_right)
{
    lv_obj_t *bar = lv_obj_create(parent);
    lv_obj_remove_style_all(bar);
    lv_obj_set_size(bar, UI_SCREEN_W, UI_TOPBAR_H);
    lv_obj_align(bar, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_set_style_bg_color(bar, UI_COLOR_PANEL, 0);
    lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(bar, UI_COLOR_LINE, 0);
    lv_obj_set_style_border_width(bar, 1, 0);
    lv_obj_set_style_border_side(bar, LV_BORDER_SIDE_BOTTOM, 0);
    lv_obj_clear_flag(bar, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *pill = lv_label_create(bar);
    lv_obj_set_style_bg_opa(pill, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(pill, 999, 0);
    lv_obj_set_style_pad_hor(pill, 8, 0);
    lv_obj_set_style_pad_ver(pill, 3, 0);
    lv_obj_set_style_text_font(pill, UI_FONT_LABEL, 0);
    lv_obj_align(pill, LV_ALIGN_LEFT_MID, 8, 0);
    ui_topbar_set_state(pill, SLP_STATE_IDLE);

    lv_obj_t *right = lv_label_create(bar);
    lv_obj_set_style_text_color(right, UI_COLOR_TEXT_DIM, 0);
    lv_obj_set_style_text_font(right, UI_FONT_LABEL, 0);
    lv_label_set_text(right, "--");
    lv_obj_align(right, LV_ALIGN_RIGHT_MID, -8, 0);

    if (out_pill) *out_pill = pill;
    if (out_right) *out_right = right;
}

void
ui_topbar_set_state(lv_obj_t *pill, enum slp_print_state state)
{
    const char *text;
    lv_color_t color;
    switch (state) {
    case SLP_STATE_HOMING:   text = "Homing";   color = UI_COLOR_STATE_HOMING; break;
    case SLP_STATE_PRINTING: text = "Printing"; color = UI_COLOR_STATE_PRINTING; break;
    case SLP_STATE_PAUSED:   text = "Paused";   color = UI_COLOR_STATE_PAUSED; break;
    case SLP_STATE_ERROR:    text = "Error";    color = UI_COLOR_STATE_ERROR; break;
    case SLP_STATE_IDLE:
    default:                 text = "Idle";     color = UI_COLOR_STATE_IDLE; break;
    }
    lv_label_set_text(pill, text);
    lv_obj_set_style_text_color(pill, color, 0);
    lv_obj_set_style_bg_color(pill, color, 0);
    lv_obj_set_style_bg_opa(pill, LV_OPA_20, 0);
}

static void
tab_click_cb(lv_event_t *e)
{
    enum ui_tab target = (enum ui_tab)(intptr_t)lv_event_get_user_data(e);
    lv_obj_t *screen = ui_screen_for_tab(target);
    if (screen)
        ui_show_screen(screen);
}

static void
make_tab(lv_obj_t *bar, int index, const char *glyph, const char *label
        , enum ui_tab active)
{
    enum ui_tab this_tab = (enum ui_tab)index;
    lv_obj_t *tab = lv_obj_create(bar);
    lv_obj_remove_style_all(tab);
    lv_obj_set_size(tab, UI_SCREEN_W / UI_TAB_COUNT, UI_TABBAR_H);
    lv_obj_align(tab, LV_ALIGN_TOP_LEFT, index * (UI_SCREEN_W / UI_TAB_COUNT), 0);
    lv_obj_add_flag(tab, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(tab, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(tab, tab_click_cb, LV_EVENT_CLICKED
                        , (void *)(intptr_t)this_tab);

    lv_color_t color = this_tab == active ? UI_COLOR_ACCENT : UI_COLOR_TEXT_FAINT;

    lv_obj_t *g = lv_label_create(tab);
    lv_label_set_text(g, glyph);
    lv_obj_set_style_text_color(g, color, 0);
    lv_obj_align(g, LV_ALIGN_TOP_MID, 0, 2);

    lv_obj_t *l = lv_label_create(tab);
    lv_label_set_text(l, label);
    lv_obj_set_style_text_color(l, color, 0);
    lv_obj_set_style_text_font(l, UI_FONT_LABEL, 0);
    lv_obj_align(l, LV_ALIGN_BOTTOM_MID, 0, -2);
}

void
ui_tabbar_create(lv_obj_t *parent, enum ui_tab active)
{
    lv_obj_t *bar = lv_obj_create(parent);
    lv_obj_remove_style_all(bar);
    lv_obj_set_size(bar, UI_SCREEN_W, UI_TABBAR_H);
    lv_obj_align(bar, LV_ALIGN_BOTTOM_LEFT, 0, 0);
    lv_obj_set_style_bg_color(bar, UI_COLOR_PANEL, 0);
    lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(bar, UI_COLOR_LINE, 0);
    lv_obj_set_style_border_width(bar, 1, 0);
    lv_obj_set_style_border_side(bar, LV_BORDER_SIDE_TOP, 0);
    lv_obj_clear_flag(bar, LV_OBJ_FLAG_SCROLLABLE);

    make_tab(bar, UI_TAB_STATUS, LV_SYMBOL_HOME, "Status", active);
    make_tab(bar, UI_TAB_JOG, LV_SYMBOL_LOOP, "Jog", active);
    make_tab(bar, UI_TAB_FILES, LV_SYMBOL_LIST, "Files", active);
    make_tab(bar, UI_TAB_MACROS, LV_SYMBOL_SETTINGS, "Macros", active);
}
