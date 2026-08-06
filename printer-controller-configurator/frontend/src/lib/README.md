# lib

## esptool-wrapper
- Wraps Espressif's `esptool-js` for in-browser flashing over Web Serial.
- Needs to handle: port selection, chip auto-detection (should work for
  P4, S3, and whatever the Touch UI/AMS chips end up being), progress
  reporting back to the UI, and clear error messaging when Web Serial
  isn't supported (non-Chromium browsers).
- Should NOT reimplement flashing logic — this is a thin wrapper around
  the existing library, per the earlier decision not to build our own
  flasher from scratch.

## board-catalog (client-side)
- Thin client for `backend/board-catalog` — fetches the current list of
  supported boards/transports/pin data rather than duplicating it.
- Keep this as close to a dumb data-fetcher as possible; the actual
  board data lives in the backend so it stays in one place.
