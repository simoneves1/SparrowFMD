# shared

Code/definitions used by more than one firmware target, to avoid the
UART message format between main-esp and touch-ui/ams-esp drifting out
of sync across separately-built firmware.

## Contents (planned)
- UART message schema (status updates, control commands) shared between
  main-esp, touch-ui, and ams-esp.
- Version/compatibility tagging so mismatched firmware versions on
  different boards fail loudly rather than silently misbehaving —
  worth deciding early given these are flashed independently via the
  configurator website.
