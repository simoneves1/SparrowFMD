# touch-ui pinout — 4.0inch ESP32-32E display

The first touch-ui bring-up board: a cheap Sunton-style "yellow display"
clone, sold as "4.0inch ESP32-32E Display" (LCD panel silkscreened
HSD-9190J-C3). Not a custom SparrowFDM board -- a bring-up/prototyping
target while the real touch-ui board is unspecified. Same convention as
main-esp/PINOUT.md: pin assignments live here, not scattered across
source comments, so they're one place to check/update.

## MCU
Plain ESP32 (module marked "ESP32-32E"), dual-core Xtensa LX6 -- **not**
an S3. touch-ui's ESP-IDF project target was switched from esp32s3 (the
original scaffold's placeholder guess) to esp32 to match this board; see
README.md's Status.

## Display -- ST7796S, 320x480 native panel, run landscape (480x320) in software, 4-wire SPI
| Function | GPIO |
|---|---|
| CS | 15 |
| DC (data/command) | 2 |
| SCK | 14 |
| MOSI | 13 |
| MISO | 12 |
| Backlight | 27 |
| Reset | tied to EN -- no separate GPIO, reset is a full chip reset |

### Orientation -- resolved by photo, not guessed
The panel is a 320x480 native part, but the UI (per the operator-UI
mockup) is landscape: `main.c` sets `DISPLAY_H_RES=480`,
`DISPLAY_V_RES=320`. This glass is bonded to the driver IC rotated
relative to its default MADCTL scan order. The correct transform is
`esp_lcd_panel_swap_xy(panel, true)` + `esp_lcd_panel_mirror(panel, true,
true)`, applied in **two places that must agree**:
`display_driver.c`'s `init_panel()` (for the pre-LVGL diagnostic and as
the panel's initial state), and `ui.c`'s `lvgl_port_display_cfg_t.rotation`
field. The second one is not optional: `esp_lvgl_port` resets the panel's
swap_xy/mirror state to whatever `rotation` says as of
`lvgl_port_add_disp()` (`lvgl_port_update_callback()`), so leaving it at
the struct's zero-init default (`false,false,false`) silently overwrites
whatever `display_driver.c` set the moment `ui_init()` runs -- every
LVGL-drawn frame ends up unrotated regardless of the panel-level call,
while a one-shot pre-LVGL diagnostic (drawn before that reset) keeps
showing the correct transform forever after, which is exactly what made
this look like a "mirror axis" bug for a while when it was actually a
"the field is unset" bug -- see the git history on these two files for
the full derivation.

Also learned the hard way, twice: solid-color content (a 4-corner
diagnostic block, or box/bar layout positions in the real UI) cannot
reveal a within-row mirror -- a solid square or a symmetric-ish card
layout looks the same forwards or backwards. Only actual text, or an
inherently asymmetric detail like tab bar reading order
(Status/Jog/Files/Macros vs. Macros/Files/Jog/Status), reveals it. Don't
trust a "looks right" verdict from solid fills or coarse layout alone --
confirm with real, asymmetric on-screen content.

Confirmed correct against real hardware: the Status screen renders
upright with correct reading order (temps, percent-complete, Pause/Stop,
and the tab bar in the right Status/Jog/Files/Macros order).

## Touch -- XPT2046 resistive, shares the display's SPI bus
| Function | GPIO |
|---|---|
| CS | 33 |
| SCK | 14 (shared with display) |
| MOSI | 13 (shared with display) |
| MISO | 12 (shared with display) |
| IRQ | 36 |

## Present on the board, not used by touch-ui yet
- SD card (SPI): CS=5, MOSI=23, SCK=18, MISO=19
- RGB status LED: R=22, G=16, B=17
- Onboard speaker/DAC: enable=4, output=26
- UART0 (USB-serial bridge, flashing/logging): RX=3, TX=1
- Battery voltage ADC: 34

## Open question
This board's UART0 is already claimed by the onboard USB-serial bridge
for flashing/logging -- the real UART link to main-esp (`uart-links`,
via `slp_uart_transport`) will need a second UART peripheral on
otherwise-free pins. None of this board's other pins are broken out to
a header beyond what's listed above; exact pin choice for that link is
still open and needs to happen before `slp_uart_transport_init` is
wired up for real (it's currently unexercised in main.c, same as
main-esp's transports).
