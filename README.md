# SparrowFDM

SparrowFDM is a printer-controller ecosystem built around one idea:
Main ESP speaks Klipper's *actual* host↔MCU protocol, not a custom one.
That means any board with existing Klipper support — Octopus, EBB36/42,
whatever — "just works" with SparrowFDM boards attached, without writing
per-board firmware. The companion configurator website (separate repo,
`printer-controller-configurator`) then generates the per-printer config
and flashes everything over WebSerial, no app install needed.

This repo is the firmware side: what runs on SparrowFDM's own boards.

- `main-esp/` — the brain (ESP32-P4). G-code parsing, kinematics,
  Klipper host protocol client, web UI + WebSocket API, SD storage.
- `touch-ui/` — local touchscreen UI, UART client to Main ESP, no
  independent kinematics/G-code logic of its own.
- `ams-esp/` — filament switcher board (deferred, not required for v1).
- `shared/` — UART message schema and other definitions used by more
  than one firmware target, so they can't drift out of sync across
  separately-built/flashed firmware.

## What this project does NOT do
It does not contain firmware for third-party boards. Those run stock,
unmodified Klipper MCU firmware, compiled by the configurator website's
build-service from Klipper's own source — not from anything in this
repo.

## Status
`main-esp`'s `klipper-host-protocol` module has its software side
done and unit-tested: message framing (VLQ, Klipper's CRC16-CCITT,
block encode/decode with resync-on-garbage), the identify handshake
(request/response + chunked dictionary reassembly), dictionary
decompression (zlib/DEFLATE + Adler-32 verification), dictionary
parsing into a queryable command/response/enum table, and generic
message encode/decode against that table. All of it is bit-for-bit
ported from Klipper's own reference implementation (klippy/msgproto.py,
klippy/chelper/msgblock.c) for wire compatibility, not reimplemented
from the protocol docs alone.

Not done: actual UART/USB transport. Everything above was built and
verified without real hardware (host-buildable unit tests + real
esp32p4 cross-compiles); transport is the first piece of this module
that genuinely needs a real board to build and validate against.

Everything else in main-esp, and all of touch-ui/ams-esp/shared, is
still just per-module design notes (see each folder's README).

## Roadmap / TODO
- [ ] Resolve the kinematics question above — prototype a float32 port
      of chelper's hot path, or get real P4 hardware/timing data
- [ ] Pick the specific P4 board/module, finalize the pin-assignment
      guide (currently blocked on that choice)
- [x] `main-esp`: `klipper-host-protocol` — framing, identify handshake,
      dictionary decompression/parsing, generic message encode/decode
- [ ] `main-esp`: `klipper-host-protocol` — UART/USB transport (needs
      real hardware to build and validate against)
- [ ] `main-esp`: `gcode-parser` module
- [ ] `main-esp`: `kinematics` module (blocked on the open question above)
- [ ] `main-esp`: `safety` module — link watchdog/heartbeat to
      mainboard/toolhead (not thermal safety, which lives on the MCU side)
- [ ] `main-esp`: `storage` module — SD mount, G-code + config file I/O
- [ ] `main-esp`: `web-ui` module — HTTP + WebSocket API shared by the
      standalone web UI, Touch UI, and (later) a farm server
- [ ] `shared`: UART message schema + version/compatibility tagging
      between independently-flashed boards
- [ ] `touch-ui`: UART client against `main-esp`'s `uart-links` module
- [ ] `ams-esp`: post-v1 — spool selector, runout sensing, swap-at-pause

## License
[GPLv3](LICENSE), matching Klipper's own license — this project speaks
Klipper's host↔MCU protocol and may incorporate or derive from Klipper's
chelper code.
