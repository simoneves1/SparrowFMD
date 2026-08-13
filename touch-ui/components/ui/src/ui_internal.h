// Private glue shared by ui.c and the screen_*.c files -- not part of
// this component's public include/ dir, only reachable from within
// components/ui/src/ itself.
#ifndef UI_INTERNAL_H
#define UI_INTERNAL_H

#include "lvgl.h"
#include "slp_messages.h"
#include "ui_chrome.h"

// Hands a command to whatever ui_send_command_fn was passed to
// ui_init(); screen button handlers call this instead of touching the
// callback pointer directly.
void ui_send_command(const struct slp_control_command *cmd);

// Thin wrapper over lv_scr_load() -- one place to add transition
// animation later if wanted, and it keeps screen_*.c files from needing
// to know LVGL's screen-load API directly.
void ui_show_screen(lv_obj_t *screen);

// Each screen module builds its own lv_obj_t (and its own top bar +
// tab bar, via ui_chrome.h) once, at ui_init() time. Navigation between
// the 4 tab screens goes through ui_screen_for_tab()/ui_screens_register()
// in ui_chrome.h instead of pairwise set_nav calls -- there's no natural
// "next screen" chain across 4 siblings the way the old print_status ->
// temperature -> filament linear flow had one.
lv_obj_t *screen_status_create(void);
void screen_status_set(const struct slp_status_update *s);

lv_obj_t *screen_jog_create(void);

// Files has no wire format yet (see ui.c's top comment) -- gallery is
// static demo data, no _set() to push real listings into.
lv_obj_t *screen_files_create(void);

lv_obj_t *screen_macros_create(void);

#endif // ui_internal.h
