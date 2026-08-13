// Display + touch bring-up for touch-ui's first hardware target: the
// 4.0inch ESP32-32E "cheap yellow display" clone (LCD panel silkscreened
// HSD-9190J-C3). See PINOUT.md for where these pin numbers come from and
// what else is on this board. Not a custom SparrowFDM board -- a
// bring-up/prototyping target, see this component's design note in
// touch-ui/README.md.
//
// *** VERIFIED ON REAL HARDWARE (panel init + fill, touch SPI comms) ***
// Flashed to a real board: the ST7796 panel initializes, a full-screen
// solid-color fill via esp_lcd_panel_draw_bitmap renders correctly (right
// color, right orientation, no swap/mirror needed -- the config below's
// defaults for this panel family turned out to be correct as-is), and
// esp_lcd_touch_read_data() completes an SPI transaction with the
// XPT2046 successfully. NOT yet verified: actual touch coordinate
// accuracy (a real finger press hasn't been checked against expected
// x/y), since that needs interactive testing beyond a boot-time
// self-test. See touch-ui/main/main.c's hw_test_display().
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
