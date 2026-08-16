# step-encoder

Turns `motion-planner`'s per-axis step events into real Klipper wire
protocol message content bytes (`queue_step` / `set_next_step_dir`), via
`klipper-host-protocol`'s `khp_msg_encode` against a `khp_msgtable`. This
is the piece `motion-planner`'s own README lists under "Not yet done."

## Status
Host-tested (22 checks, see `test/test_main.c`) against a mock
dictionary using Klipper's real, documented `queue_step`/
`set_next_step_dir` format strings (from Klipper's own
`src/stepper.c`), and cross-compiles clean for esp32p4/esp32s3 as part
of `main-esp`'s normal build. Wired into `main.c`'s boot self-test
(against the same mock dictionary), so a real board also confirms one
step event encodes to real Klipper wire-format bytes -- not run against
a real MCU's actual dictionary, since there's no real MCU board yet
(see `HARDWARE_TESTING.md`), and a real dictionary's `queue_step`
msgid/format is only known via that board's identify handshake.

## Scope (deliberately minimal, matching this project's established pattern)
- **No run-length step compression.** Real Klipper's `queue_step` is an
  `interval`/`count`/`add` triple that can describe a whole run of
  evenly-spaced steps in one message -- that's `stepcompress.c`'s job in
  real Klipper, a genuinely separate, substantial compression algorithm
  (bisection search for the widest valid interval/add pair covering a
  run of steps). This project's `kinematics_steps.h` already documents
  that it doesn't implement that either. `step-encoder` emits one
  `queue_step` per step event, always `count=1 add=0` -- a legitimate,
  valid special case of the real wire format (a real MCU accepts and
  executes it correctly), just not compressed. Revisit if/when real
  hardware throughput data shows this matters.
- **Clock quantization is `time * mcu_freq_hz`, rounded to the nearest
  tick**, with no wraparound handling -- real Klipper's clock is a
  32-bit counter that wraps and needs periodic resynchronization
  (`clock_updater` in klippy). Not implemented; fine for the timescales
  a single G-code file exercises, would need addressing for continuous
  long-running operation.
- **`oid` (per-axis stepper object id) and `mcu_freq_hz` are caller-
  supplied**, not derived from anything. Real values only exist once a
  real MCU's identify dictionary has been parsed (the dictionary's
  `config` key holds `CLOCK_FREQ`; `oid`s are assigned by the printer's
  stepper pin configuration, which itself doesn't exist as a concept in
  this repo yet). `khp_msgtable.h` explicitly notes the dictionary's
  `config` key is still out of scope.
- **Direction changes are detected per axis and re-emit
  `set_next_step_dir`** before the next `queue_step`, matching real
  Klipper's protocol (a stepper's direction pin must be set before the
  step pulses that use it are queued).

## Not yet done
- Running against a real MCU's actual dictionary instead of a mock one
  -- see `HARDWARE_TESTING.md`'s "Once a real Klipper MCU board is
  available" section, which is where this pipeline was always expected
  to become testable end to end.
- Run-length step compression (see Scope above).
- Clock wraparound/resync.
- Actually transmitting the encoded messages anywhere -- this only
  produces message content bytes; framing them into wire blocks is
  `khp_msgblock`'s job, and sending them is `khp_transport`'s (UART or
  USB CDC), neither of which this component touches.
