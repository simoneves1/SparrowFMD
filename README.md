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

A transport skeleton also exists now: `khp_session` (receive buffering,
the framing/dispatch loop, outgoing sequencing) is transport-agnostic
and unit-tested with a mock transport, and `khp_uart_transport` is a
concrete ESP-IDF UART implementation of it. That UART transport is
explicitly **unverified against real hardware** — it cross-compiles,
nothing more — and it's the wrong transport for talking to third-party
boards anyway (that needs USB CDC-ACM, not UART; the UART transport
serves the touch-ui/ams-esp `uart-links` case instead). USB CDC-ACM for
Octopus/toolhead-board communication is still a separate, unstarted,
meaningfully larger piece of work.

`gcode-parser` is also done and unit-tested: turns a line of G-code
text into a command word + parameters, staying deliberately ignorant of
what any command means (per its own design note) and falling back to
raw text for commands like M117 that don't use standard letter=value
params.

`shared-protocol` (in `shared/shared-protocol/`) is also done and
unit-tested: the UART message schema between main-esp and touch-ui/
ams-esp, with its own frame envelope (independent from
klipper-host-protocol's -- this is SparrowFDM's own protocol, no
external wire-compatibility constraint) and a per-frame protocol
version byte so mismatched firmware on independently-flashed boards
fails loudly rather than silently misbehaving, per the module's stated
design goal. First concrete message types: status_update and
control_command.

Everything else in main-esp, and all of touch-ui/ams-esp, is still just
per-module design notes (see each folder's README) -- touch-ui and
ams-esp specifically are waiting on their own ESP-IDF projects to exist
before they can actually consume shared-protocol.

## Roadmap / TODO
- [ ] Resolve the kinematics question above — prototype a float32 port
      of chelper's hot path, or get real P4 hardware/timing data
- [ ] Pick the specific P4 board/module, finalize the pin-assignment
      guide (currently blocked on that choice)
- [x] `main-esp`: `klipper-host-protocol` — framing, identify handshake,
      dictionary decompression/parsing, generic message encode/decode
- [x] `main-esp`: `klipper-host-protocol` — transport skeleton
      (khp_session, tested; khp_uart_transport, unverified against
      hardware — see Status)
- [ ] `main-esp`: `klipper-host-protocol` — USB CDC-ACM transport for
      talking to third-party boards (Octopus, toolhead boards); UART
      transport above doesn't satisfy this
- [ ] Validate `khp_uart_transport` and the rest of
      `klipper-host-protocol` against real hardware once available
- [x] `main-esp`: `gcode-parser` module
- [ ] `main-esp`: `kinematics` module (blocked on the open question above)
- [ ] `main-esp`: `safety` module — link watchdog/heartbeat to
      mainboard/toolhead (not thermal safety, which lives on the MCU side)
- [ ] `main-esp`: `storage` module — SD mount, G-code + config file I/O
- [ ] `main-esp`: `web-ui` module — HTTP + WebSocket API shared by the
      standalone web UI, Touch UI, and (later) a farm server
- [x] `shared`: UART message schema + version/compatibility tagging
      between independently-flashed boards
- [ ] `touch-ui`: UART client against `main-esp`'s `uart-links` module
- [ ] `ams-esp`: post-v1 — spool selector, runout sensing, swap-at-pause

## License
[GPLv3](LICENSE), matching Klipper's own license — this project speaks
Klipper's host↔MCU protocol and may incorporate or derive from Klipper's
chelper code.
