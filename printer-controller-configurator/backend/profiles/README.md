# profiles

Lets someone save a config once and apply it across multiple printers —
built for the farm use case (many identical units, same base config)
rather than the one-off single-printer flash flow the rest of the
backend was originally designed around.

## What a profile is
A named, saved config bundle: board selection, kinematics type, pin
mapping, motor current settings, transport choices per link — everything
that's genuinely identical across a batch of same-model printers.

## Critical: split fields into "template" vs "per-unit"
Not everything in a config is safe to copy across units. Blindly cloning
a full config from unit #1 to units #2-#20 will carry over values that
are physically specific to unit #1's hardware and are wrong for
everyone else. The schema needs an explicit split, not an implicit one:

- **Template fields** (safe to share across a profile): board models,
  kinematics type, pin mapping, transport per link, general motor
  current defaults.
- **Per-unit fields** (must NOT be copied blindly — flag for
  re-calibration after flashing, every time): probe Z-offset, PID-tuned
  heater values, individual stepper current fine-tuning if per-unit
  tuned, input-shaping resonance frequencies.

When a profile is applied to a new unit, per-unit fields should either
be left blank/unset (forcing a calibration step) or clearly marked as
"copied from template, verify before printing" in the UI — never
silently applied as if they were already correct for the new unit.

## Storage
This is the first part of the backend that needs to persist data across
sessions (board-catalog and build-service are otherwise stateless per
request). Needs real storage — not decided yet whether that's a small
database or just structured files; low priority to resolve early since
it doesn't block anything else.

## UI entry point (see frontend/src/pages)
"Start from a saved profile" should be an alternate entry point into the
existing board-select -> connection-select -> review-and-flash flow —
skips straight to review-and-flash with template fields pre-filled and
per-unit fields explicitly called out as needing attention.
