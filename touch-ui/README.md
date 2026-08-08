# touch-ui

Firmware for the touchscreen ESP32 board — targets ESP32-S3 (common
choice for display/touch boards: PSRAM for framebuffers, good LCD/touch
peripheral support). UART client to main-esp — no independent
kinematics/G-code logic of its own, purely a display + input front-end.

## Status
Scaffolding stage: a real ESP-IDF project exists (`idf.py build` from
this directory), consuming `shared/shared-protocol`'s frame encode/
decode (see that module's own status). No display/touch driver, no
actual UART wiring to a real main-esp yet — `main.c` just self-tests
shared-protocol's encode/decode path on boot. `slp_uart_transport` (in
shared-protocol) is the real transport this will use once wired up, and
like every other ESP-IDF-glue piece in this project it's currently
unverified against real hardware.

## Contents (planned)
- UART client talking to main-esp's `uart-links` module — status
  display (temps, progress), basic controls (start/stop/pause, jog).
- Should mirror the same control surface as main-esp's web UI where
  possible rather than defining a separate, divergent feature set.
- Keep this firmware simple/replaceable — a build without a touchscreen
  attached should be a fully supported configuration (v1 scope note:
  standalone units need to work with just the web UI too).
