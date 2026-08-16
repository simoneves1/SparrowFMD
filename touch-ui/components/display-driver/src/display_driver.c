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

// Derived from real hardware -- see touch-ui/main/main.c's
// run_touch_calibration() and this file's init_touch() comment. A first
// 2-point (diagonal) calibration attempt produced constants that looked
// plausible but felt orientation-wrong in practice; a 3-point (L-shaped,
// non-colinear) recalibration revealed the raw X/Y axes are genuinely
// swapped on this unit -- moving only the logical X target barely moved
// raw X (delta ~0) but moved raw Y by 241, i.e. raw Y is the one that
// actually tracks logical X. These constants apply that swap before the
// per-axis scale/offset.
#define TOUCH_CAL_SWAP_RAW_XY 1
#define TOUCH_CAL_SCALE_X   1.65975f
#define TOUCH_CAL_OFFSET_X -29.71f
#define TOUCH_CAL_SCALE_Y   0.76433f
#define TOUCH_CAL_OFFSET_Y -17.32f

static uint16_t
clamp_u16(float v, uint16_t max)
{
    if (v < 0.f)
        return 0;
    if (v > (float)max)
        return max;
    return (uint16_t)v;
}

static void
touch_apply_calibration(esp_lcd_touch_handle_t tp, uint16_t *x, uint16_t *y
                        , uint16_t *strength, uint8_t *point_num
                        , uint8_t max_point_num)
{
    (void)tp; (void)strength; (void)max_point_num;
    for (uint8_t i = 0; i < *point_num; i++) {
#if TOUCH_CAL_SWAP_RAW_XY
        float rx = y[i], ry = x[i];
#else
        float rx = x[i], ry = y[i];
#endif
        float lx = rx * TOUCH_CAL_SCALE_X + TOUCH_CAL_OFFSET_X;
        float ly = ry * TOUCH_CAL_SCALE_Y + TOUCH_CAL_OFFSET_Y;
        // Hardcoded to this board's known 480x320 landscape resolution
        // (touch-ui/main/main.c's DISPLAY_H_RES/DISPLAY_V_RES) rather
        // than threaded through from cfg -- this callback's signature is
        // fixed by esp_lcd_touch_config_t and has no spare context
        // pointer wired up for it here.
        x[i] = clamp_u16(lx, 479);
        y[i] = clamp_u16(ly, 319);
    }
}

static bool
init_touch(struct display_driver_handles *out
          , const struct display_driver_config *cfg)
{
    esp_lcd_panel_io_spi_config_t io_config = ESP_LCD_TOUCH_IO_SPI_XPT2046_CONFIG(cfg->pin_touch_cs);
    if (esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)cfg->spi_host
                                 , &io_config, &out->touch_io) != ESP_OK)
        return false;

    // The XPT2046 is a *resistive* touch controller: its raw ADC output
    // range is a property of this specific physical panel's resistive
    // gradient (manufacturing tolerance), not something swap_xy/mirror_x/
    // mirror_y flags alone can fix -- those only reorder/flip axes, they
    // don't rescale the raw range. Found via real hardware: with those
    // flags set, taps still landed nowhere near their visual target (e.g.
    // tapping the Jog tab visually did nothing), and a 2-point
    // calibration (draw a marker at a known logical position, log the
    // raw reading for a real finger press there, repeat for a second
    // point far away) showed the raw range wasn't close to
    // 0..x_max/0..y_max at all. See touch-ui/main/main.c's
    // run_touch_calibration() for how these constants were derived --
    // they're specific to this exact physical unit's touch panel, not a
    // general XPT2046 property; a different physical board of the same
    // model would need its own recalibration.
    esp_lcd_touch_config_t touch_config = {
        .x_max = cfg->h_res,
        .y_max = cfg->v_res,
        .rst_gpio_num = -1, // no separate touch reset line on this board
        .int_gpio_num = cfg->pin_touch_irq,
        .process_coordinates = touch_apply_calibration,
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
