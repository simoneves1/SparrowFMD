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

## Designed to not lock into one chip
Only one board is actually planned (P4), but nothing in this codebase's
own source is P4-specific — no hardcoded pins, no `#ifdef
CONFIG_IDF_TARGET_ESP32P4` anywhere in `components/`. Chip-specific
defaults (e.g. `storage`'s SDMMC slot pinout) come from ESP-IDF's own
per-chip macros, not anything hand-rolled here, so switching the build
target picks up the right values automatically. This is deliberate: if
the P4 kinematics question (see the root README) forces a different
chip, or a future board revision wants to move to something else, the
application code shouldn't need to change — only the build target and
the pin-assignment guide.

As a standing check that this stays true rather than eroding over time,
this project builds for a second target — `esp32s3` — alongside the
real `esp32p4` target, in a separate build directory with its own
sdkconfig so the two never interfere:

```
idf.py build                                          # esp32p4 (the real target)
idf.py -B build.s3 -D SDKCONFIG=sdkconfig.s3 build     # esp32s3 (portability check)
```

Both currently build clean, zero warnings, no source changes needed
between them. That doesn't mean the firmware *works* on an S3 (nothing
here has run on real hardware of any kind yet — see each ESP-IDF-glue
file's own "unverified against real hardware" note), only that the
application logic isn't accidentally coupled to P4-specific behavior.
