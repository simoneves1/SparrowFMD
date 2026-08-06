# printer-controller-configurator

The website: lets someone design their printer's board setup (which mainboard,
toolhead board, touch UI, optional AMS, and which transport each link uses),
generates the right firmware/config for each board, and flashes them —
no separate app install required.

Two halves:
- `frontend/` — the config wizard + in-browser flashing (WebSerial/esptool-js)
- `backend/` — compiles Klipper MCU firmware for whichever third-party boards
  (Octopus, EBB36/42, RAMPS, etc.) the user selected

## What this project does NOT do
It does not contain the firmware that runs on our own boards (Main ESP,
Touch UI, AMS). That firmware lives in the separate `printer-controller-firmware`
repo and ships as pre-built release binaries — this site just flashes those
releases onto the ESP boards and generates the printer's config file for them.

## Relationship to Klipper
For third-party mainboards/toolhead boards, we are NOT writing our own MCU
firmware. We compile and ship stock Klipper MCU firmware for those boards
unmodified — our Main ESP speaks Klipper's own host<->MCU protocol, so any
board with existing Klipper support works without us doing per-board firmware
work. See `backend/build-service/README.md`.

## Open decisions (resolve before writing code)
- Chrome/Edge-only limitation from Web Serial — acceptable, or need a fallback?
- Where the build backend actually runs (self-hosted server vs. serverless
  build jobs) — affects how "free" this site is to operate at scale.
- Versioning story: what happens when we update Main ESP firmware — does the
  site need to detect and prompt for re-flashing later, or is that out of
  scope for v1?
