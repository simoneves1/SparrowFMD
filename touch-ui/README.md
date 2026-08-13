# touch-ui

Firmware for the touchscreen board — UART client to main-esp, no
independent kinematics/G-code logic of its own, purely a display + input
front-end.

## Status
Targets plain esp32 (retargeted from an initial esp32s3 placeholder
guess once real hardware was picked -- see PINOUT.md). First bring-up
target is a cheap Sunton-style "yellow display" clone, sold as "4.0inch
ESP32-32E Display" (320x480 ST7796 panel, XPT2046 resistive touch, both
on a shared SPI bus) -- not a custom SparrowFDM board, just what's on
hand to bring the display/touch side up on. PINOUT.md has the full pin
list and what else is on this board.

A real ESP-IDF project exists (`idf.py build` from this directory),
consuming `shared/shared-protocol`'s frame encode/decode (see that
module's own status) and a new `display-driver` component
(`components/display-driver`) that wraps `esp_lcd_st7796` and
`esp_lcd_touch_xpt2046` (managed components) behind a small
`display_driver_init()` call: sets up the shared SPI bus, resets/inits
the panel, and brings up the touch controller.

Unlike most ESP-IDF-glue pieces in this project, this one **has** been
flashed and run on the real board repeatedly. `main.c`'s
`hw_test_display()` brings up `display_driver_init()` and draws a
4-corner color diagnostic (not a solid fill -- a solid fill can't reveal
a rotation bug, since a rotated solid fill still looks like a solid
fill; this is exactly how an earlier solid-red version of this same
test missed a real orientation bug that only became visible once actual
asymmetric screen content was rendered). That diagnostic, read against
real photos of the physical panel twice, resolved a genuine hardware
finding: **this panel's glass is bonded to the ST7796 driver IC rotated
relative to its default MADCTL scan order.** `display-driver`'s
`init_panel()` now applies `esp_lcd_panel_swap_xy(panel, true)` +
`esp_lcd_panel_mirror(panel, true, false)` to correct it -- without
this, content renders transposed/rotated 90 degrees. See PINOUT.md's
"Orientation" section for the full derivation.

The touch-ui screens were originally built portrait (320x480, a guess
made before any real design existed) and have since been **redesigned
around a real design pass**: the "SparrowFDM -- Operator UI mockup"
(a separate design-review artifact, not code in this repo) specifies a
landscape 480x320 layout with a 4-tab bottom nav -- Status, Jog, Files,
Macros -- and a specific dark teal/ember color language. `main.c` now
runs `DISPLAY_H_RES=480`/`DISPLAY_V_RES=320` to match; the same
swap_xy/mirror_x transform found above turned out to be correct for
both orientations (it's a fixed property of the glass bonding,
independent of which axis software calls "width" -- see PINOUT.md).

`components/ui` was rebuilt from scratch around that mockup:
`ui_theme.h` centralizes the color palette and Montserrat font sizes
(14/20/24, enabled in `sdkconfig` -- only 14 was enabled before, not
enough for the mockup's actual size hierarchy), `ui_chrome.c` builds
the shared top status-pill bar and bottom tab bar every screen reuses,
and four `screen_*.c` files build Status (temp cards + progress +
pause/stop, replacing the old single-column print-status/temperature
screens), Jog (3x3 XY pad + vertical Z cluster + home/start -- no
step-size picker, matching the mockup's own note that touch drops it),
Files (a gallery grid + tap-through print-confirm subpage), and Macros
(6-button grid: filament load/unload and temp presets are real
`slp_control_command`s; "Bed mesh" has no corresponding command in
`slp_messages.h` at all and is intentionally a no-op rather than
sending something misleading). The old numeric-keypad temp-entry screen
from the portrait design is gone -- the mockup replaced it with fixed
presets, so keeping `lv_keyboard` wired up would've been scope the
actual design doesn't call for.

**Hardware-verified**, including real bugs caught only by looking at the
physical screen, not just by a clean boot log: with all 5 screens
(Status/Jog/Files-gallery/Files-detail/Macros) built eagerly at boot,
LVGL's internal memory pool -- 32KB, sized for the old 3-screen portrait
design -- ran out partway through `screen_macros_create()`, hanging the
boot task badly enough to trip the watchdog for 15+ seconds at the exact
same allocation call every time (not a slow-progress symptom, a genuine
stuck allocator); fixed by bumping `CONFIG_LV_MEM_SIZE_KILOBYTES` from
32 to 96 (checked against this board's actual free-heap numbers in the
boot log first, not guessed).

A boot log alone couldn't catch what came next, because it looks
identical whether the screen is readable or not: once actually
photographed, the real UI rendered as color-shifted noise, then (after
fixing that) upside-down, then (after fixing that) left-right mirrored.
Three separate real bugs, found and fixed in sequence against real
hardware, not guessed: (1) `CONFIG_LV_COLOR_16_SWAP` was unset, so
LVGL's RGB565 buffers were byte-order-mismatched against what this
ST7796 panel expects over SPI -- raw diagnostic draws (which bypass
LVGL) looked fine the whole time, masking this; (2)/(3) see PINOUT.md's
Orientation section for the swap_xy/mirror derivation and the
`esp_lvgl_port` gotcha behind it (a config field being unset silently
reset the panel's orientation on every LVGL frame). The Status screen
now renders correctly on real hardware: temps, progress, Pause/Stop, and
the tab bar in the right reading order.

`main.c` still drives Status with one canned `slp_status_update` at
boot (no real UART link to main-esp yet, same open PINOUT.md question
as before); button presses build a real `slp_control_command` and log
it rather than sending it anywhere, for the same reason. Not yet
verified: actual touch coordinate accuracy and tab navigation from a
real finger press (needs interactive testing, not just a boot-time
self-test) -- next up.

## Contents (planned)
- UART client talking to main-esp's `uart-links` module — status
  display (temps, progress), basic controls (start/stop/pause, jog).
- Should mirror the same control surface as main-esp's web UI where
  possible rather than defining a separate, divergent feature set.
- Keep this firmware simple/replaceable — a build without a touchscreen
  attached should be a fully supported configuration (v1 scope note:
  standalone units need to work with just the web UI too).
