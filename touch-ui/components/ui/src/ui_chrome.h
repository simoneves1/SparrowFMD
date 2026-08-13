// Shared chrome -- the top status bar and bottom 4-tab nav bar that
// appear on every screen (mockup's "screen-top"/"tab-bar" elements).
// Pulled out once instead of five copy-pasted implementations.
#ifndef UI_CHROME_H
#define UI_CHROME_H

#include "lvgl.h"
#include "slp_messages.h"

enum ui_tab {
    UI_TAB_STATUS = 0,
    UI_TAB_JOG,
    UI_TAB_FILES,
    UI_TAB_MACROS,
    UI_TAB_COUNT,
};

// Registry of the 4 tab-target screens, populated once in ui.c after all
// screen_*_create() calls return. Tab bars don't hold direct pointers to
// each other's screens -- they look this up by index at tap time, so
// screen modules never need to know about each other.
void ui_screens_register(lv_obj_t *status, lv_obj_t *jog
                         , lv_obj_t *files, lv_obj_t *macros);
lv_obj_t *ui_screen_for_tab(enum ui_tab tab);

// Top bar: a state pill (left) whose text/color tracks slp_print_state,
// and a free-form right-side label (elapsed time, file count, or a
// "back" link depending on the screen -- callers set its text directly).
// Either out-param may be NULL if a screen doesn't need to update it
// after creation (e.g. Macros' right label is static "--").
void ui_topbar_create(lv_obj_t *parent, lv_obj_t **out_pill
                      , lv_obj_t **out_right);
void ui_topbar_set_state(lv_obj_t *pill, enum slp_print_state state);

// Bottom 4-tab nav bar, highlighting `active`. Tapping any other tab
// calls ui_show_screen() on ui_screen_for_tab()'s result -- built once
// per screen (not shared across screens as a single moved widget, since
// each LVGL screen is its own object tree).
void ui_tabbar_create(lv_obj_t *parent, enum ui_tab active);

#endif // ui_chrome.h
