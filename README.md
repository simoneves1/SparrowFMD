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
Planning/scaffolding stage — no firmware code written yet, just
per-module design notes (see each folder's README) and one resolved
research spike (below). The immediate next step is starting on
`main-esp`'s `klipper-host-protocol` module, since it's the piece the
"any board just works" story depends on.

## Open question: does full Klipper-grade kinematics fit on the P4?
A benchmark spike (chelper cross-compiled for the P4, since removed from
this repo — the finding stands regardless) found a concrete, structural
reason to be skeptical: the P4's RISC-V core has no double-precision
hardware FPU, and Klipper's chelper does all its trajectory math in
`double`. A real cross-compile confirmed every double-precision op in
chelper's hot path becomes a software-floating-point library call, not a
hardware instruction. No real on-target timing exists yet to say whether
that's actually too slow — the open question is now "does a float32 port
of chelper's hot path close the gap, or do we fall back to Marlin-style
local stepping" rather than "will chelper run fast enough."

## Roadmap / TODO
- [ ] Resolve the kinematics question above — prototype a float32 port
      of chelper's hot path, or get real P4 hardware/timing data
- [ ] Pick the specific P4 board/module, finalize the pin-assignment
      guide (currently blocked on that choice)
- [ ] `main-esp`: `klipper-host-protocol` module — talks to third-party
      boards over USB using Klipper's real wire protocol
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
