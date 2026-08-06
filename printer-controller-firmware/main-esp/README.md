# main-esp

Firmware for the ESP32-P4 "brain" board. Targets ESP-IDF.

See `src/README.md` for the module breakdown. See the pin-assignment
guide (separate doc) for GPIO planning — needs to be finalized once the
specific P4 board/module is confirmed.

## Runtime config, not compile-time
Per-printer settings (which boards are attached, kinematics type, pin
mapping to external boards, calibration values) load from a config file
at runtime — same philosophy as Klipper's `printer.cfg`. The
configurator website generates this file; main-esp firmware itself does
not need to be recompiled per printer. Keep it this way — baking
per-printer config into the firmware build would break the "flash once,
reuse across similar printers" story and duplicate the whole point of
having a config file at all.
