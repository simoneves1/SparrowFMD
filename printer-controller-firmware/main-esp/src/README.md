# src — module breakdown

Notes on what each module should contain once implementation starts.
Naming is provisional.

## gcode-parser
Parses incoming G-code (from SD or network upload) into an internal move
list. Keep this decoupled from kinematics — parser shouldn't need to
know which kinematics type is active.

## kinematics
Trajectory planning / lookahead. THE module most affected by the
pending benchmark result. Two designs to keep in mind while structuring
this, so switching doesn't mean a rewrite:
  - Full Klipper-grade: jerk-limited lookahead, junction deviation,
    pressure advance integrated into planning.
  - Marlin-style fallback: Bresenham-style local stepping, lighter
    lookahead, less CPU.
Scope for v1: single kinematics type only (Cartesian or CoreXY, match
whatever the test printer actually is) — no multi-kinematics abstraction
yet, don't over-engineer this before the benchmark result is in.

## klipper-host-protocol
Client implementation of Klipper's actual host<->MCU wire protocol.
Talks to Octopus (USB) and toolhead board (USB) using this. This is the
module that makes multi-board-type support "free" — get this right and
matching against Klipper's real protocol spec, don't drift from it.

## safety
Watchdog/heartbeat logic against the mainboard/toolhead link. NOTE:
thermal runaway protection itself lives on the mainboard/toolhead
firmware (i.e., in Klipper's own MCU code, not here) — this module's job
is detecting a stale/dead link from the Main ESP side and forcing a safe
state, not re-implementing thermal safety redundantly.

## storage
SD card (SDMMC) mount, G-code file read/write, config file load.

## web-ui
HTTP + WebSocket server. Serves the local standalone UI and exposes the
same API the Touch UI and (later) farm server use — per the earlier
decision that standalone and farm-managed modes should share one control
surface rather than diverging.

## uart-links
Touch UI and AMS communication. Two independent UART peripherals, each
point-to-point — see the wiring guide for why these can't share a bus.

## network
WiFi client (via the P4/C5 companion-chip SDIO link, if that module is
used) — farm server communication, G-code upload endpoint. Should be
optional/gracefully-absent for pure standalone operation.
