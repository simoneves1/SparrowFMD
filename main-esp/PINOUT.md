# main-esp pin-assignment guide — JC-ESP32P4-M3-DEV

Development board picked for `main-esp`: **Guition JC-ESP32P4-M3-DEV**
(ESP32-P4 + ESP32-C6 companion, 32MB PSRAM, 16MB flash). Source of truth
for anything not listed here is the vendor's own schematic —
`DRubioG/JC-ESP32P4-M3-DEV` on GitHub, `5-Schematic/schematic.pdf` and
`5-Schematic/imgs/*.png`. This doc only covers the pins `main-esp`
firmware actually touches; it is not a full board reference.

Per `README.md`'s "designed to not lock into one chip" note: none of
these pin numbers belong in `components/` application code. They're
board-specific config values passed in at the point each transport/driver
is initialized (UART port config, TWAI driver config, etc.), the same way
`storage`'s SDMMC slot pinout already comes from ESP-IDF's per-chip
defaults rather than a hardcoded constant. If the board changes, this
file changes — the modules under `components/` should not need to.

## Expansion header (`JP1`) — pins this project assigns

The header exposes 8 general-purpose GPIOs (everything else on it is
either power/ground, the codec/touch I2C bus, or reserved for the C6
companion chip — see "Not available" below). Assignment:

| Signal | GPIO | Consumer |
|---|---|---|
| Touch-UI UART TX | GPIO1 | `uart-links` — point-to-point to `touch-ui` |
| Touch-UI UART RX | GPIO2 | |
| AMS UART TX | GPIO3 | `uart-links` — reserved now, `ams-esp` is deferred |
| AMS UART RX | GPIO4 | |
| TWAI_TX (CAN) | GPIO32 | to CAN transceiver TXD (see below) |
| TWAI_RX (CAN) | GPIO33 | to CAN transceiver RXD |
| spare | GPIO5 | unassigned |
| spare | GPIO20 | unassigned |

None of GPIO1–5/20/32/33 are strapping pins on this board — the boot-mode
strap is GPIO35, wired internally to the BOOTMODE button, not exposed on
this header. Safe to use as plain GPIO from cold boot.

### CAN transceiver wiring (SN65HVD230 or equivalent 3.3V transceiver)

```
Header                SN65HVD230 module
VCC3V3  ──────────────  VCC
GND     ──────────────  GND
GPIO32  ──────────────  TXD
GPIO33  ──────────────  RXD
                         Rs   → GND   (high-speed mode)
                         CANH ──┐
                         CANL ──┤  → twisted pair to the other CAN node
```

120Ω termination at the two physical bus ends only, not at every node.
Bitrate: 500 kbit/s (Klipper/Katapult's standard CAN bridge speed).

**Do not use an MCP2515-based module or a 5V-logic transceiver (e.g.
TJA1050)** — the P4 has a native TWAI controller so an SPI-CAN bridge
chip is unnecessary complexity, and the header's GPIOs are 3.3V-only.

## USB — three Type-C ports, not interchangeable

| Connector (schematic label) | P4 net | Function |
|---|---|---|
| "High speed USB" | `ESP_USB_P/N` | The **only** USB host-capable port (P4's native USB 2.0 OTG HS controller). This is what `klipper-host-protocol`'s USB transport must use to talk to Octopus/toolhead boards. |
| "Full Speed USB" | `USB1P1_P/N` | P4's built-in USB-Serial-JTAG peripheral. Device-only — flashing/console, cannot host another board. |
| Third Type-C (labeled `USB1`, via CH340C) | UART0 (`GPIO37`/`GPIO38`) | Classic USB-to-serial for flashing/console over UART0. Also device-only. |

**Open problem, not yet resolved:** the host-protocol design needs to
talk to *two* independent USB devices (Octopus + toolhead board), but
this board exposes exactly one USB host controller. Options: an external
USB hub off the HS port (relies on ESP-IDF's USB Host Library enumerating
multiple downstream devices), or moving one of the two links off USB.
Not blocking UART/CAN bring-up — a software `khp_usb_cdc_transport`
skeleton now exists (see main README's Status), but this decision is
still needed before it can drive both boards on this specific module.

## Onboard, not header — no pin planning needed

- **WiFi/BT**: via onboard ESP32-C6 over SDIO (ESP-Hosted style link).
  Configured through `esp_wifi_remote`/hosted component config, not raw
  GPIO numbers here.
- **SD card**: dedicated SDMMC pins routed directly from P4 to an onboard
  TF card slot (schematic sheet `3_8311&TFCARD.png`). **Exact GPIO
  numbers not yet transcribed here** — the schematic labels these nets
  `SD_CMD`/`SD_CLK`/`SD_DATA0-3` without listing the underlying GPIO
  number on the same page, and none of this repo's own IDF-DEMO
  reference code is genuinely board-specific (the `bsp_extra` folders in
  the vendor demo repo pull in `espressif__esp32_p4_function_ev_board`
  and `waveshare__esp32_p4_nano` components — BSPs for *different*
  boards, not usable as a pin reference for this one). Pull the real
  numbers from `5-Schematic/schematic.pdf` directly before wiring up
  `storage_sd`'s slot config.
- **Ethernet (RJ45)**: RMII pins fixed at GPIO28–35/49–52 per the
  schematic's `4_ESP32P4.png` page — available if the network module
  wants wired Ethernet instead of/alongside WiFi, no header pins consumed.

## Not available on the header (reserved elsewhere on the board)

- `ES_I2C_SDA`/`ES_I2C_SCL` (GPIO7/GPIO8) — shared bus already used by
  the onboard audio codec (ES8311) and capacitive touch controller.
  Usable if a device can share that bus, but don't assume it's free.
- `C6_U0RXD`/`C6_U0TXD`, `C6_IO9`, `C6_CHIP_PU` — control/programming
  lines for the ESP32-C6 companion module, not general-purpose from the
  P4's side.
