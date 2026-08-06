# touch-ui

Firmware for the touchscreen ESP32 board. UART client to main-esp — no
independent kinematics/G-code logic of its own, purely a display +
input front-end.

## Contents (planned)
- UART client talking to main-esp's `uart-links` module — status
  display (temps, progress), basic controls (start/stop/pause, jog).
- Should mirror the same control surface as main-esp's web UI where
  possible rather than defining a separate, divergent feature set.
- Keep this firmware simple/replaceable — a build without a touchscreen
  attached should be a fully supported configuration (v1 scope note:
  standalone units need to work with just the web UI too).
