// Shared color palette and layout constants for all touch-ui screens,
// lifted from the operator-UI mockup's dark-theme CSS tokens (the
// mockup defines both a light and dark palette via CSS variables --
// firmware has no concept of OS light/dark mode, so this hardcodes the
// dark set, which is what the mockup's screenshots actually render as
// on a device screen). Keeping every screen_*.c file pulling colors from
// here instead of inlining hex values is what keeps them visually
// consistent as one system rather than five independently-guessed UIs.
//
// Font sizes: the mockup's --font-label (Bahnschrift/DIN Alternate) and
// --font-data (monospace) distinction isn't replicated -- pulling in
// custom LVGL font assets is a separate, larger piece of work not
// attempted here. Everything uses the built-in Montserrat at three
// sizes (14/20/24, enabled in sdkconfig) for a real size hierarchy
// without custom font files.
#ifndef UI_THEME_H
#define UI_THEME_H

#include "lvgl.h"

#define UI_COLOR_INK        lv_color_hex(0xeef1ef) // primary text
#define UI_COLOR_PANEL      lv_color_hex(0x23272a) // card background
#define UI_COLOR_PANEL_2    lv_color_hex(0x1b1e21) // sunken/secondary surface
#define UI_COLOR_PAPER      lv_color_hex(0x16181a) // screen background
#define UI_COLOR_LINE       lv_color_hex(0x34393c) // borders
#define UI_COLOR_TEXT_DIM   lv_color_hex(0xa7afac)
#define UI_COLOR_TEXT_FAINT lv_color_hex(0x737b78)

#define UI_COLOR_ACCENT     lv_color_hex(0x2dd4bf) // teal -- primary actions, progress
#define UI_COLOR_ACCENT_INK lv_color_hex(0x06201c) // text on top of accent-filled buttons
#define UI_COLOR_EMBER      lv_color_hex(0xff8a4c) // temperature readouts

#define UI_COLOR_STATE_IDLE     lv_color_hex(0x8a938f)
#define UI_COLOR_STATE_HOMING   lv_color_hex(0x6ea8f0)
#define UI_COLOR_STATE_PRINTING lv_color_hex(0x2dd4bf)
#define UI_COLOR_STATE_PAUSED   lv_color_hex(0xf0b94a)
#define UI_COLOR_STATE_ERROR    lv_color_hex(0xf2695f)

#define UI_FONT_BODY  (&lv_font_montserrat_14)
#define UI_FONT_LABEL (&lv_font_montserrat_14)
#define UI_FONT_VALUE (&lv_font_montserrat_20)
#define UI_FONT_HERO  (&lv_font_montserrat_24)

// Landscape canvas, matches DISPLAY_H_RES/V_RES in main.c.
#define UI_SCREEN_W 480
#define UI_SCREEN_H 320

#define UI_TOPBAR_H 30
#define UI_TABBAR_H 34
#define UI_BODY_Y   UI_TOPBAR_H
#define UI_BODY_H   (UI_SCREEN_H - UI_TOPBAR_H - UI_TABBAR_H)

// Applies the shared card look (panel background, border, rounded
// corners) -- every card-like container across all screens goes through
// this instead of repeating the same four style calls five times.
static inline void
ui_style_card(lv_obj_t *obj)
{
    lv_obj_set_style_bg_color(obj, UI_COLOR_PANEL, 0);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(obj, UI_COLOR_LINE, 0);
    lv_obj_set_style_border_width(obj, 1, 0);
    lv_obj_set_style_radius(obj, 10, 0);
    lv_obj_set_style_pad_all(obj, 8, 0);
}

#endif // ui_theme.h
