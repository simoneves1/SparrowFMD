# printer-controller-firmware

The firmware that runs on OUR boards: Main ESP, Touch UI, and (later) the
AMS board. Ships as versioned release binaries that the configurator
website flashes via WebSerial.

Three firmware targets, one shared library:
- `main-esp/` — the brain: G-code parsing, kinematics, Klipper host
  protocol client, web UI + WebSocket API, SD storage
- `touch-ui/` — local touchscreen UI, UART client to Main ESP
- `ams-esp/` — filament switcher (deferred, not required for v1)
- `shared/` — protocol/message definitions used by more than one target

## What this project does NOT do
It does not contain firmware for third-party boards (Octopus, EBB36/42,
etc.) — those run stock, unmodified Klipper MCU firmware, compiled by
the configurator website's build-service from Klipper's own source, not
from anything in this repo.

## Foundational decision this whole repo depends on
Main ESP implements Klipper's actual host<->MCU protocol (not a custom
one) when talking to third-party boards. This is what makes "support
lots of board types" nearly free — see the architecture planning notes.

## Big open question blocking real implementation — narrowed, not resolved
The kinematics benchmark (chelper cross-compiled for the P4) has been run
as a spike — see `main-esp/benchmarks/chelper-p4/RESULTS.md`. No P4
hardware or working emulator was available, so there's no real on-target
timing number yet, but the spike found something concrete: the P4's
RISC-V core has no double-precision hardware FPU, and Klipper's chelper
does all trajectory math in `double`. A real cross-compile for `esp32p4`
confirms every double-precision op in the hot path becomes a
software-floating-point library call, not a hardware instruction. That's
a specific, structural reason to be skeptical of "full Klipper-grade
kinematics fits as-is" — not a final answer, but the open question is now
"does a float32 port of chelper's hot path close the gap, or do we need
the Marlin-style fallback" rather than "will chelper run fast enough."
