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
serves the touch-ui/ams-esp `uart-links` case instead).

A USB CDC-ACM transport (`khp_usb_cdc_transport`) now exists for that
Octopus/toolhead-board case: it layers ESP-IDF's built-in `usb` (USB
Host Library) component and Espressif's managed `usb_host_cdc_acm`
class driver behind the same `khp_transport` interface `khp_uart_transport`
implements, so `khp_session` doesn't need to care which one it's talking
to. It cross-compiles for both esp32p4 and esp32s3 (component-manager
dependency resolves and builds clean for both), but carries the same
**unverified against real hardware** caveat — no real USB device has
been enumerated against it, nothing has run on a real Klipper MCU
board's CDC-ACM endpoint. Device selection is by VID/PID (or
`CDC_HOST_ANY_*` for a single point-to-point link with no hub); only
one USB link per process is supported for now.

`gcode-parser` is also done and unit-tested: turns a line of G-code
text into a command word + parameters, staying deliberately ignorant of
what any command means (per its own design note) and falling back to
raw text for commands like M117 that don't use standard letter=value
params.

`motion-planner` (new) is the first thing in this repo that actually
links gcode text to real step output: turns parsed `G0`/`G1` commands
into queued `kinematics` moves and reports the resulting stepper pulse
events, closing the gap `gcode-parser`'s own design note leaves on
purpose ("stays decoupled from kinematics"). Deliberately minimal --
only `G0`/`G1` act, no lookahead queue (every move fully stops before
the next starts), `F`/X/Y/Z are sticky/modal per standard G-code
behavior. Host-tested (12 checks) and wired into `main-esp/main.c`'s
boot self-test, so a real board also runs one real G1 line through the
real parser and planner and confirms steps come out the other end. A
real bug was caught during development this way: an early version's
flush-time margin (copied from `kinematics`' own single-move test
harness) silently dropped steps for any queued move shorter than that
margin -- see that component's own README.md for the full story and how
the regression test catches it. Not yet done: feeding step events into
`klipper-host-protocol`'s message encoding to actually reach a real MCU
board, and any real lookahead/cornering-speed smoothing.

`shared-protocol` (in `shared/shared-protocol/`) is also done and
unit-tested: the UART message schema between main-esp and touch-ui/
ams-esp, with its own frame envelope (independent from
klipper-host-protocol's -- this is SparrowFDM's own protocol, no
external wire-compatibility constraint) and a per-frame protocol
version byte so mismatched firmware on independently-flashed boards
fails loudly rather than silently misbehaving, per the module's stated
design goal. Concrete message types: status_update and control_command.
It now also has its own transport-agnostic session layer
(`slp_session`, unit-tested with a mock transport, mirroring
klipper-host-protocol's `khp_session`) and a concrete ESP-IDF UART
transport (`slp_uart_transport`, unverified against real hardware) --
this one, unlike `khp_uart_transport`, *is* the real transport for its
protocol, not a stand-in.

`safety`'s `link_watchdog` is also done and unit-tested: a small,
generic heartbeat monitor (feed it on each message from a link, check()
it periodically) that reports a latched fault once a link's gone quiet
too long, with an edge-triggered callback firing exactly once at the
transition. Explicitly not thermal runaway protection, per the
module's own design note -- that lives on the mainboard/toolhead's own
firmware, not here.

`storage`'s `cfg_parser` is also done and unit-tested: reads main-esp's
per-printer runtime config file (INI-style like Klipper's own
printer.cfg, for familiarity -- but main-esp's own format, no
compatibility requirement to match it byte-for-byte). Its `storage_sd`
SD-card mount is a concrete ESP-IDF SDMMC+FATFS skeleton with the same
**unverified against real hardware** caveat as `khp_uart_transport` --
cross-compiles, nothing more.

`web-ui`'s JSON WebSocket/HTTP API (`web_api`) has grown well past the
original status/control_command parity with shared-protocol, unit-tested
throughout (40+ host-buildable test cases): status now carries
filename/layer/remaining-time and Tune-panel state (speed/flow
factor, Z-offset); control_command covers start-with-filename,
temp-set/filament commands (matching touch-ui's Macros screen), raw
"gcode" lines for a Console page, and the 3 Tune commands; three plain
HTTP endpoints (`GET /api/files`, `GET/POST /api/camera`,
`GET/POST /api/settings`) round out file listing, camera source
configuration, and raw printer.cfg-style config text, all documented in
`web_api.h`'s top comment. `web_server.c` (the concrete ESP-IDF
`esp_http_server` implementation) has the same **unverified against real
hardware** caveat as `khp_uart_transport`/`storage_sd` -- it cross-compiles
for both esp32p4 and esp32s3, and (new) `main-esp/main/main.c` now
actually calls `web_server_start()` on boot with real callbacks wired to
`cfg_parser` (in-RAM only, not persisted) rather than leaving it
permanently uncalled -- but the server is still unreachable until a
network driver (WiFi or Ethernet) is wired up, which hasn't happened
yet. `web-ui/webapp/index.html` (a real, self-contained HTML/CSS/JS
frontend, no build step, no framework, embedded into the firmware image
via `EMBED_TXTFILES`) grew alongside the API: a tabbed layout (Status --
temps/progress/print control/Tune/Camera/Jog/Macros together, per
explicit direction to keep those grouped rather than fragmented across
tabs -- Files, Console, Settings), a Files gallery (tap a tile to start
that file), a Console with a live response log, and a Settings page with
both a structured Camera config form and the raw config-text editor.
Verified end-to-end against a hand-rolled local mock WebSocket/HTTP
server (not ESP-IDF, a throwaway Node script) driving the real page in a
real browser -- confirms the frontend/JSON contract works together, but
is not a substitute for a real device test, which hasn't happened yet.

Since then: every "command" message now gets a `command_ack` reply
(reached-the-server confirmation, not "the printer did it" -- there's
still no gcode/kinematics pipeline to report that) surfaced as toast
notifications in the frontend; `GET /api/history` and `GET /api/diagnostics`
round out the HTTP endpoints (print history and `safety`'s
`link_watchdog` link status, both backed by real logic in `main.c` --
history records a real "stopped early" entry off a real received "stop",
diagnostics reports a real link_watchdog that genuinely transitions
OK -> FAULTED ~10s after boot since no real UART receive loop feeds it
yet, not a fabricated demo value); Tune moved off the Status page onto
Settings so it can't be bumped by accident; and the Stop button now asks
for confirmation before sending, the one control on this page with a
real, hard-to-undo consequence.

`touch-ui` now has a real ESP-IDF project (retargeted from an initial
esp32s3 guess to plain esp32, matching the first real hardware picked --
see touch-ui/README.md and touch-ui/PINOUT.md) that builds clean and
consumes shared-protocol, plus a `display-driver` component wrapping
managed `esp_lcd_st7796`/`esp_lcd_touch_xpt2046` drivers for that
hardware (a 4.0inch ESP32-32E "cheap yellow display" clone). Flashed and
run on the real board repeatedly, including finding and fixing two real
hardware/config issues rather than just a first-try success: (1) this
specific panel's glass is bonded rotated relative to the ST7796 driver's
default scan order -- found via a 4-corner color diagnostic (a solid
fill can't reveal rotation; a rotated solid fill still looks solid) read
against real photos, fixed with `esp_lcd_panel_swap_xy`+`mirror` in
`display-driver`; (2) the `ui` component was redesigned from an initial
portrait 3-screen guess to match a real design pass -- the "SparrowFDM
Operator UI mockup" -- as landscape 480x320 with a 4-tab nav (Status,
Jog, Files, Macros), which then exposed LVGL's 32KB memory pool (sized
for the old simpler design) running out mid-boot and hanging the
watchdog; bumped to 96KB against this board's actual free-heap numbers,
confirmed fixed. Status and Jog are wired to real `slp_control_command`s
(pause/resume/stop, jog/home/start); Files and Macros are partly UI-only
stubs where `slp_messages.h` has no wire format yet (file listings, a
"bed mesh" command) -- see touch-ui/README.md for exactly which parts
are real vs. stubbed. No real UART wiring
to main-esp yet -- that needs a second UART peripheral pin choice on
this board (its UART0 is claimed by the USB-serial bridge), and
main-esp's own board (P4) hasn't been purchased yet. `ams-esp` remains
post-v1 design notes only.

Everything else in main-esp is still just per-module design notes (see
`main-esp/src/README.md`).

A float32 prototype of chelper's hot path (`trapq`/`itersolve`/
`kin_cartesian`, mechanically ported from Klipper's `double`-based source)
was tried as a possible fix for the P4's lack of hardware double-precision
FPU. A first attempt hit a real correctness problem: `itersolve.c`'s
convergence epsilons are calibrated for double precision (1e-9), which is
unrepresentable in float32 at realistic bed-scale coordinates/print
durations; loosening them to a single fixed, representable value (~1e-4)
avoided that but caused ~80% step-count inflation (spurious/duplicate step
commands), because that epsilon wasn't tight enough relative to a real
microstepping `half_step` distance. A second attempt replaced the fixed
epsilon with an adaptive one -- the larger of a tolerance relative to
`half_step` and float32's own representable-ULP floor at the magnitude in
play -- and **resolved it**: a host-native sanity sweep across realistic bed
sizes (0-500mm) and print durations (start of print to 72 hours in) matched
the expected step count/timing to within a fraction of a percent, with
relative step-to-step spacing confirmed magnitude-independent (Klipper's
own algorithm rebases to move-local time before the precision-sensitive
math, insulating it from how large absolute print_time has grown). Only the
*absolute* step timestamp carries a small, bounded, unavoidable storage
error from print_time living in a float32 struct field (tens to a couple
hundred microseconds over a multi-hour print) -- a known, separate property,
not shown to affect motion smoothness. **Verdict: the correctness blocker
is resolved** -- the open question below is back to being purely a speed
question (does it run fast enough on the P4's hardware float32 FPU). That
speed question is deliberately on hold: the QEMU install here only supports
the C3 machine, and the C3 has no hardware FPU at all, so timing this
float32 port under C3/QEMU would only measure software emulation speed --
not the P4 hardware-FPU behavior this whole prototype exists to test, so it
wouldn't be a trustworthy answer. Static disassembly-based cycle estimation
was considered as a fallback but also deferred -- real P4 hardware, once
available, gives a real measurement instead of an estimate either way.
Unlike the first (negative) prototype, this one's code has since been
promoted out of the benchmark spike into a real, permanent
`main-esp/components/kinematics` module (Cartesian only, matching this
project's stated scope) -- host-tested (the same bed-size/print-duration
sweeps that validated the adaptive-epsilon fix, now as real PASS/FAIL
checks, see that component's own `test/test_main.c`) and cross-compiles
clean for both esp32p4 and esp32s3 as part of `main-esp`'s normal build.
Not yet wired to anything: there's no `gcode-parser` -> `kinematics` ->
step-transmission pipeline yet, and the speed question above is still
open pending real P4 hardware -- this only means the trajectory-planning
piece of that future pipeline is real, tested code now instead of a
throwaway spike. See that component's own README.md for the full
provenance/scope writeup.

Only one board is actually planned (P4), but see main-esp/README.md's
"Designed to not lock into one chip" note: nothing in the application
code is P4-specific, and main-esp now builds clean for a second chip
(`esp32s3`) as a standing check that stays true. Not a claim that the
firmware *works* on an S3 -- nothing has run on real hardware yet -- just
that switching chips, if the P4 kinematics question forces it, shouldn't
require rewriting application logic.

## Roadmap / TODO
- [x] Prototype a float32 port of chelper's hot path — first attempt hit
      a correctness blocker, second attempt (adaptive epsilon) resolved
      it, see Status
- [x] Promote the float32 kinematics port from a benchmark spike into a
      real `main-esp/components/kinematics` module — Cartesian only,
      host-tested, cross-compiles for esp32p4/esp32s3, see Status
- [ ] Resolve the kinematics speed question above — get real P4 timing
      data for the float32 port (deliberately waiting for real hardware
      rather than an emulated/estimated number, see Status — the
      available QEMU can't exercise the P4's hardware FPU)
- [x] Wire `gcode-parser` -> `kinematics` — new `motion-planner` module,
      G0/G1 only, no lookahead queue, host-tested (12 checks) and wired
      into main.c's boot self-test, see Status
- [ ] Wire `motion-planner`'s step events -> a real step-transmission
      path (`kinematics_steps.h`'s callback -> `klipper-host-protocol`
      message encoding) — no such pipeline exists yet
- [x] Pick the specific P4 board/module for development — Guition
      JC-ESP32P4-M3-DEV; UART-link and CAN pin assignments done, see
      `main-esp/PINOUT.md`. SD/Ethernet exact pin numbers and the
      dual-USB-host-port question (Octopus + toolhead both need USB host)
      are still open
- [x] `main-esp`: `klipper-host-protocol` — framing, identify handshake,
      dictionary decompression/parsing, generic message encode/decode
- [x] `main-esp`: `klipper-host-protocol` — transport skeleton
      (khp_session, tested; khp_uart_transport, unverified against
      hardware — see Status)
- [x] `main-esp`: `klipper-host-protocol` — USB CDC-ACM transport
      (`khp_usb_cdc_transport`) for talking to third-party boards
      (Octopus, toolhead boards); cross-compiles for esp32p4/esp32s3,
      unverified against real hardware — see Status
- [ ] Validate `khp_uart_transport`, `khp_usb_cdc_transport`, and the
      rest of `klipper-host-protocol` against real hardware once
      available
- [x] `main-esp`: `gcode-parser` module
- [x] `main-esp`: `motion-planner` module — G0/G1 -> kinematics moves,
      host-tested, wired into main.c's boot self-test, see Status
- [x] `main-esp`: `kinematics` module — Cartesian trajectory planning,
      host-tested, cross-compiles for esp32p4/esp32s3; not yet wired to
      `gcode-parser` or a real step-transmission path, see Status
- [x] `main-esp`: `safety` module — link watchdog/heartbeat to
      mainboard/toolhead (not thermal safety, which lives on the MCU side)
- [x] `main-esp`: `storage` module — cfg_parser tested; SD mount
      unverified against hardware — see Status
- [ ] Validate `storage_sd` against real hardware once available
- [x] `main-esp`: `web-ui` module — JSON WebSocket/HTTP API tested
      (status/control_command, files/camera/settings endpoints,
      Console/Tune commands); full tabbed frontend (Status/Files/Console/
      Settings) built and mock-server-verified; `main.c` now starts the
      server on boot, see Status
- [ ] Wire a network driver (WiFi or Ethernet) so the web server is
      actually reachable — currently starts but binds to nothing
- [ ] Validate `web_server` + the frontend against real hardware/a real
      browser once available
- [x] `shared`: UART message schema + version/compatibility tagging
      between independently-flashed boards
- [x] `shared-protocol`: transport-agnostic session layer (tested) +
      concrete UART transport (unverified against hardware)
- [x] `touch-ui`: real ESP-IDF project, retargeted esp32s3 → esp32 to
      match first real hardware, consumes shared-protocol
- [x] `touch-ui`: `display-driver` component (ST7796 panel +
      XPT2046 touch, 4.0inch ESP32-32E board) — flashed and run on real
      hardware: panel init + full-screen fill render correctly, touch
      controller responds over SPI. Orientation initially looked correct
      from a solid fill, but that was a false negative -- a rotated
      solid fill still looks solid. A 4-corner color diagnostic (see
      Status) found this panel's glass really is bonded rotated;
      `esp_lcd_panel_swap_xy`+`mirror` in `display-driver` now corrects
      it, confirmed against two real photos (portrait, then again after
      the landscape pivot below). Touch coordinate accuracy from a real
      finger press now verified too (see below)
- [x] `touch-ui`: `ui` component — redesigned from an initial portrait
      3-screen guess to landscape 480x320 with a 4-tab nav (Status, Jog,
      Files, Macros) matching the "SparrowFDM Operator UI mockup" design
      pass; `ui_theme.h`/`ui_chrome.c` centralize the palette and shared
      top/tab bars. Status and Jog wired to real `slp_control_command`s;
      Files/Macros partly UI-only where `slp_messages.h` has no wire
      format yet (file listings, "bed mesh"). Build- and
      hardware-verified: fixed an LVGL-v8-vs-v9 `esp_lvgl_port` struct
      field mismatch, found and fixed the panel orientation bug above,
      then found and fixed LVGL's 32KB memory pool (sized for the old
      design) running out mid-boot and hanging the watchdog once all 5
      screens were built eagerly -- bumped to 96KB. Boot log now
      confirms all 4 tab screens build and the canned demo status
      renders, no crash
- [x] Interactive verification on the physical panel: tapping did
      nothing at first -- the XPT2046 touch controller has its own
      independent swap_xy/mirror config (separate from the display
      panel's), and beyond that, being a *resistive* controller, its raw
      ADC range needed a real per-device linear calibration (a first
      2-point diagonal attempt couldn't detect an axis swap that turned
      out to be genuinely present on this unit; redone with 3
      non-colinear points). Applied via `esp_lcd_touch_config_t`'s
      `process_coordinates` hook. Tab navigation and button hits now
      land where tapped on real hardware.
- [ ] `touch-ui`: real UART client against `main-esp`'s `uart-links`
      module — needs a second UART peripheral pin choice, see
      touch-ui/PINOUT.md's open question, and main-esp's own board (P4)
      hasn't been purchased yet so this can't be tested end-to-end regardless
- [ ] `ams-esp`: post-v1 — spool selector, runout sensing, swap-at-pause

## License
[GPLv3](LICENSE), matching Klipper's own license — this project speaks
Klipper's host↔MCU protocol and may incorporate or derive from Klipper's
chelper code.
22