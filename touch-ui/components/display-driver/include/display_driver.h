// Display + touch bring-up for touch-ui's first hardware target: the
// 4.0inch ESP32-32E "cheap yellow display" clone (LCD panel silkscreened
// HSD-9190J-C3). See PINOUT.md for where these pin numbers come from and
// what else is on this board. Not a custom SparrowFDM board -- a
// bring-up/prototyping target, see this component's design note in
// touch-ui/README.md.
//
// *** VERIFIED ON REAL HARDWARE, including touch accuracy ***
// Panel: correct color/orientation confirmed on real hardware (needed a
// real swap_xy+mirror(x,y) transform plus CONFIG_LV_COLOR_16_SWAP for
// LVGL content specifically -- see touch-ui/PINOUT.md's Orientation
// section and touch-ui/README.md for the full derivation, including the
// esp_lvgl_port gotcha where an unset rotation field silently overwrote
// this file's panel-level transform).
// Touch: the XPT2046 is a *resistive* controller, so raw coordinates
// needed a real per-device linear calibration (scale+offset per axis,
// plus an axis swap that turned out to be genuinely present on this
// unit) applied via esp_lcd_touch_config_t's process_coordinates hook
// below -- fixed swap_xy/mirror flags alone aren't enough for a
// resistive panel's raw ADC range. Derived from real finger-press data
// via touch-ui/main/main.c's run_touch_calibration() (see its own
// comment for why a 3-point, non-diagonal calibration was needed instead
// of 2 -- a 2-point diagonal fit can't detect an axis swap). Confirmed
// correct: tab navigation and button hits land where tapped.
#ifndef DISPLAY_DRIVER_H
#define DISPLAY_DRIVER_H

#include <stdbool.h>
#include "driver/spi_master.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_touch.h"

struct display_driver_config {
    spi_host_device_t spi_host; // LCD and touch share this bus (separate CS lines)
    int pin_sclk;
    int pin_mosi;
    int pin_miso;

    int pin_lcd_cs;
    int pin_lcd_dc;
    int pin_lcd_bl; // backlight, driven as a plain on/off GPIO for now

    int pin_touch_cs;
    int pin_touch_irq;

    int h_res;
    int v_res;
};

struct display_driver_handles {
    esp_lcd_panel_io_handle_t lcd_io;
    esp_lcd_panel_handle_t panel;
    esp_lcd_panel_io_handle_t touch_io;
    esp_lcd_touch_handle_t touch;
};

// Initializes the shared SPI bus, then the ST7796 panel (reset + init +
// display on) and the XPT2046 touch controller on top of it, filling
// *out with the resulting handles. Returns false if any step fails.
// Like khp_uart_transport_init and friends in main-esp, nothing here is
// ever torn down -- a "flash once" runtime-config model.
bool display_driver_init(struct display_driver_handles *out
                         , const struct display_driver_config *cfg);

#endif // display_driver.h
