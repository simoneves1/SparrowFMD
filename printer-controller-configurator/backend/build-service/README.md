# build-service

Takes a board selection (from the frontend) and produces the correct
compiled firmware.

## Scope reminder
This only builds firmware for THIRD-PARTY boards (Octopus, EBB36/42,
RAMPS/AVR boards, etc.) — it compiles their existing, unmodified Klipper
MCU firmware with the right board/pin config selected, the same thing
Klipper's own `make menuconfig` + `make` does today, just automated.

It does NOT build Main ESP / Touch UI / AMS firmware — those are our own
firmware repo's pre-built releases, not compiled per-user.

## Per-architecture toolchain needs
- AVR boards (RAMPS/Mega-based): needs `avr-gcc` toolchain
- STM32 boards (Octopus, EBB36/42): needs `arm-none-eabi-gcc` toolchain
- Reuse Klipper's actual build system/Kconfig rather than reinventing a
  parallel one — it already parameterizes exactly this kind of
  per-board, per-architecture build.

## Output per board type
- AVR / serial-bootloader boards: a `.bin` (or `.hex`, TBD) sized for
  serial-bootloader flashing
- STM32 SD-card-flash boards (BTT convention): a `.bin` named
  `firmware.bin` for the user to place on the SD card root

## Open questions
- Where builds actually run (dedicated build server, containerized CI-style
  job, etc.) — not decided.
- Caching: most users will pick from a fairly small set of common board
  combinations — worth caching compiled output per (board, pin-config)
  combination rather than recompiling from scratch every time.
