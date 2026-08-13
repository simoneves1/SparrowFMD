// Screen layer for touch-ui, built on LVGL via Espressif's esp_lvgl_port
// (the standard glue between esp_lcd_panel/esp_lcd_touch handles -- what
// display-driver already hands back -- and LVGL's own display/input
// devices). Landscape 480x320, four tab screens -- Status, Jog, Files,
// Macros -- plus a Files print-confirm subpage, redesigned around the
// "SparrowFDM -- Operator UI mockup" (see touch-ui/README.md). Two of
// the four are backed by real shared-protocol commands end to end
// (Status: pause/resume/stop; Jog: jog/home/start); Files and Macros
// are UI-only in places -- Files has no wire format for a real listing
// yet (static demo data, and "Print" can't tell main-esp *which* file),
// and Macros' "Bed mesh" button has no corresponding command at all
// (intentionally a no-op rather than sending something misleading) --
// see each screen_*.c file's own header comment for specifics.
//
// Build-verified (idf.py build compiles clean) and hardware-verified
// for the previous 3-screen portrait design's boot path -- this
// redesign inherits that same ui_init()/ui_status_update() entry point
// and lvgl_port setup, not yet re-flashed itself, see touch-ui/README.md.
#ifndef UI_H
#define UI_H

#include <stdbool.h>
#include "display_driver.h"
#include "slp_messages.h"

// Called whenever a screen turns a button press into a control_command
// (pause/resume/stop, a temp-set, a filament move). touch-ui has no real
// transport wired to main-esp yet (see PINOUT.md's open UART pin
// question), so for now the caller-supplied implementation just encodes
// and logs it, the same way main.c's self_test_shared_protocol() does --
// this callback is the seam a real slp_uart_transport send will plug
// into later.
typedef void (*ui_send_command_fn)(const struct slp_control_command *cmd
                                   , void *ctx);

struct ui_config {
    struct display_driver_handles display; // from display_driver_init()
    int h_res;
    int v_res;
    ui_send_command_fn send_command;
    void *send_command_ctx;
};

// Brings up LVGL against the given display/touch handles, builds all
// three screens, and shows the print-status screen. Returns false if
// LVGL port init fails.
bool ui_init(const struct ui_config *cfg);

// Pushes fresh data into whichever screen(s) display it (currently
// print-status and temperature). Safe to call from outside LVGL's own
// task -- takes the lvgl_port lock internally.
void ui_status_update(const struct slp_status_update *s);

#endif // ui.h
