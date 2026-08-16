# motion-planner

Turns parsed G-code (from `gcode-parser`) into queued kinematics moves
(via `kinematics`) and reports the resulting stepper pulse events. This
is the piece `gcode-parser`'s own design note explicitly says doesn't
belong there ("stays decoupled from kinematics on purpose") -- this
component is that consumer, and the first thing in this repo that
actually links gcode text to real step output.

## Status
Host-tested (12 checks, see `test/test_main.c`) and cross-compiles clean
for esp32p4/esp32s3 as part of `main-esp`'s normal build. Wired into
`main-esp/main/main.c`'s boot self-test, so a real board also exercises
one real `G1` line through the real parser and planner and confirms
steps came out the other end -- not a performance measurement, just
confirmation the chain links together on the real target.

## Scope (deliberately minimal)
- Only `G0`/`G1` are acted on. Everything else (`G28` homing, `G92`
  offset, M-codes, etc.) is silently ignored -- this proves the chain
  works, it is not a G-code interpreter.
- No lookahead queue: every move decelerates fully to a stop before the
  next one starts. Real Klipper smooths velocity across queued moves and
  limits cornering speed by junction angle; this doesn't, on purpose, to
  keep this first wiring pass tractable. Motion is correct, just not how
  a real print should feel -- every move has a dead stop before the
  next. Revisit once this base chain is verified end to end (including,
  eventually, on real hardware with real timing).
- `G0`/`G1` treated identically (no rapid/feed distinction).
- `F` is sticky across commands (standard G-code behavior), X/Y/Z
  omitted from a line keep their current value ("modal" behavior).

## A real bug this caught during development
The first version computed each move's `itersolve_generate_steps()`
flush time as `time_cursor + 1 second` -- copied from the kinematics
module's own test harness, which only ever queued one isolated move per
run, so the extra margin was harmless there. Here, moves queue
back-to-back: if a move's own duration is under that margin (common for
short real G-code moves), the *next* call's `last_flush_time`
bookkeeping looks like it already covers past that next move's end,
silently dropping its steps entirely. Fixed by using the move's own
exact end time with no margin (`itersolve_gen_steps_range()` already
clamps internally, so no margin is needed for correctness within one
move either). `test_multiple_short_moves_none_dropped()` in
`test/test_main.c` catches this regression -- confirmed by deliberately
reintroducing the bug and watching that test (and 3 others) fail.

## Not yet done
- Feeding `kinematics_steps`' per-axis step events into
  `klipper-host-protocol`'s message encoding, to actually transmit them
  to a real MCU board. Currently the caller just gets a callback per
  step (or `main.c`'s self-test just counts them).
- A real lookahead queue (velocity smoothing + junction/cornering limits
  across consecutive moves) -- see "Scope" above.
- `G28`/`G92` and anything else beyond `G0`/`G1`.
