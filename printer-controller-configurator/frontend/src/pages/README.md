# pages

One file per wizard step. Notes on intended contents — no code yet.

## start-from-profile (new entry point)
Alternate starting point for the wizard, for farm builds — lets the
user pick a saved profile (see `backend/profiles`) instead of building a
config from scratch. Skips straight to review-and-flash with template
fields pre-filled. Per-unit fields (probe offset, PID values, etc.) must
be visibly flagged as unset/needing calibration, not silently carried
over from whatever unit the profile was originally saved from.

## board-select
- Pulls its list of options from `backend/board-catalog` (don't hardcode
  board data in the frontend — it needs to match what the build service
  can actually compile for).
- Selections needed: Main ESP variant, mainboard model, toolhead board
  model, touch UI (fixed for v1, may become optional later), AMS
  (optional — can be skipped entirely per the v1 scope decision).
- Each selected board should show its known transport options (e.g. "this
  toolhead board supports USB or CAN") sourced from the catalog, not
  hand-maintained here.

## connection-select
- For each link between boards, let the user pick the transport if more
  than one option exists (e.g. toolhead board: USB vs CAN).
- Should visually mirror the wiring diagrams from the architecture
  planning doc — reuse those as reference when building this UI, not just
  a bare form.
- Validate combinations against known-bad pairings before allowing
  "generate" (e.g. flag the Octopus CAN-bridge caveat if somehow selected
  alongside other USB devices in a way that conflicts).

## review-and-flash
- Shows a summary of the full selected build before doing anything
  irreversible.
- Branches into per-board-type flashing flow:
  - ESP boards (Main ESP, Touch UI, AMS): in-browser WebSerial flash via
    `lib/esptool-wrapper`, no file download needed.
  - SD-card-flash boards (BTT Octopus and similar): serve the generated
    `firmware.bin` for download + on-screen instructions (drop on SD
    card root, power cycle).
  - Serial-bootloader boards (older AVR/RAMPS-style): needs its own
    flashing flow — different protocol than esptool, TBD which library
    to use here (research needed, not decided yet).
