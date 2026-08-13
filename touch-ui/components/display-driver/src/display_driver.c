// See display_driver.h -- unverified against real hardware.
#include "display_driver.h"
#include "esp_lcd_st7796.h"
#include "esp_lcd_touch_xpt2046.h"
#include "driver/gpio.h"

// Dropped from 40MHz: the LCD and touch controller share one SPI bus on
// this board, and 40MHz produced visible pixel corruption once real
// content (fine text/many small color regions) was drawn -- the earlier
// orientation test's big solid-color blocks were too forgiving of
// occasional bit errors to catch this, the same lesson as that test's own
// "solid fill can't reveal rotation" note applied to signal integrity
// instead of orientation.
#define DISPLAY_DRIVER_LCD_PCLK_HZ (20 * 1000 * 1000)

static bool
init_backlight(int pin_lcd_bl)
{
    gpio_config_t bl_cfg = {
        .pin_bit_mask = 1ULL << pin_lcd_bl,
        .mode = GPIO_MODE_OUTPUT,
    };
    if (gpio_config(&bl_cfg) != ESP_OK)
        return false;
    return gpio_set_level(pin_lcd_bl, 1) == ESP_OK;
}

static bool
init_panel(struct display_driver_handles *out
          , const struct display_driver_config *cfg)
{
    esp_lcd_panel_io_spi_config_t io_config = {
        .cs_gpio_num = cfg->pin_lcd_cs,
        .dc_gpio_num = cfg->pin_lcd_dc,
        .spi_mode = 0,
        .pclk_hz = DISPLAY_DRIVER_LCD_PCLK_HZ,
        .trans_queue_depth = 10,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
    };
    if (esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)cfg->spi_host
                                 , &io_config, &out->lcd_io) != ESP_OK)
        return false;

    esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = -1, // tied to EN on this board, see PINOUT.md
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = 16,
    };
    if (esp_lcd_new_panel_st7796(out->lcd_io, &panel_config, &out->panel) != ESP_OK)
        return false;

    if (esp_lcd_panel_reset(out->panel) != ESP_OK)
        return false;
    if (esp_lcd_panel_init(out->panel) != ESP_OK)
        return false;

    // This panel's glass is bonded rotated 90d CW relative to the ST7796
    // driver IC's default row/column addressing -- confirmed by solving
    // the 4-corner diagnostic in main.c's hw_test_display() against a
    // real photo: logical (0,0)/(320,0)/(0,480)/(320,480) landed at
    // physical top-right/bottom-right/top-left/bottom-left respectively,
    // a clean 90d CW rotation with no additional mirroring. Working that
    // through the driver's MADCTL semantics (MV=swap_xy, MX=mirror_x,
    // MY=mirror_y; panel_st7796_swap_xy/mirror in
    // esp_lcd_st7796_general.c) gives swap_xy + mirror_x as the exact
    // correcting transform. (A blind first guess landed on this same
    // combination but didn't visibly change anything on real hardware --
    // most likely that flash never actually took, given the repeated
    // COM-port/stuck-monitor-process issues around that same test; this
    // is now derived from real coordinate data, not a guess.)
    // swap_xy=true + mirror(x=true,y=true). mirror(false,true) got the
    // frame upright (no longer 180d off) but still left-right mirrored --
    // easy to miss from box positions/bars alone (same blind spot as the
    // very first rotation bug: symmetric-ish layout elements don't reveal
    // a mirror, only reading the tab bar order -- Macros/Files/Jog/Status
    // instead of Status/Jog/Files/Macros -- and the backwards text gave
    // it away). Adding mirror_x=true on top of the existing mirror_y=true
    // corrects that remaining left-right flip.
    if (esp_lcd_panel_swap_xy(out->panel, true) != ESP_OK)
        return false;
    if (esp_lcd_panel_mirror(out->panel, true, true) != ESP_OK)
        return false;

    return esp_lcd_panel_disp_on_off(out->panel, true) == ESP_OK;
}

static bool
init_touch(struct display_driver_handles *out
          , const struct display_driver_config *cfg)
{
    esp_lcd_panel_io_spi_config_t io_config = ESP_LCD_TOUCH_IO_SPI_XPT2046_CONFIG(cfg->pin_touch_cs);
    if (esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)cfg->spi_host
                                 , &io_config, &out->touch_io) != ESP_OK)
        return false;

    esp_lcd_touch_config_t touch_config = {
        .x_max = cfg->h_res,
        .y_max = cfg->v_res,
        .rst_gpio_num = -1, // no separate touch reset line on this board
        .int_gpio_num = cfg->pin_touch_irq,
    };
    return esp_lcd_touch_new_spi_xpt2046(out->touch_io, &touch_config
                                         , &out->touch) == ESP_OK;
}

bool
display_driver_init(struct display_driver_handles *out
                    , const struct display_driver_config *cfg)
{
    if (!init_backlight(cfg->pin_lcd_bl))
        return false;

    spi_bus_config_t bus_config = {
        .sclk_io_num = cfg->pin_sclk,
        .mosi_io_num = cfg->pin_mosi,
        .miso_io_num = cfg->pin_miso,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = cfg->h_res * cfg->v_res * (int)sizeof(uint16_t),
    };
    if (spi_bus_initialize(cfg->spi_host, &bus_config, SPI_DMA_CH_AUTO) != ESP_OK)
        return false;

    return init_panel(out, cfg) && init_touch(out, cfg);
}
