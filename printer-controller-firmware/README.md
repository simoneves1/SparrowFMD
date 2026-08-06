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

## Big open question blocking real implementation
The kinematics benchmark (chelper cross-compiled for the P4, tested
against real G-code) hasn't been run yet. Until that's done, `main-esp`
below is written assuming full Klipper-grade kinematics fit on the P4 —
if the benchmark says otherwise, the kinematics/planning module design
changes to the Marlin-style local-execution fallback discussed earlier.
