# board-catalog

The data source of truth for "what boards do we support and how do we
talk to them." Both the frontend wizard and the build-service read from
this rather than each maintaining their own copy.

## Per-board data needed
- Board name/model, architecture (AVR/STM32/ESP), supported transports
  (USB/UART/CAN), flash method (WebSerial / SD-card self-flash / serial
  bootloader)
- Pin alias table — reuse published Klipper community pin mappings where
  they exist rather than re-deriving them by hand
- Any known caveats to surface in the UI (e.g. the Octopus CAN-bridge
  mode issue documented in the architecture notes)

## Where this data should come from
Klipper's own project already publishes board definitions and pin
mappings for a huge list of boards — this catalog should pull from /
stay aligned with that rather than being hand-maintained from scratch.
Worth deciding: mirror their data at build time, or maintain our own
curated subset covering just the boards we've actually tested.

## v1 scope
Only needs entries for the boards actually decided so far: BTT Octopus
V1.1, BTT EBB36/EBB42. Everything else (RAMPS, other AVR boards, other
STM32 mainboards) is a v2 catalog-expansion task, not required to get
v1 working end to end.
