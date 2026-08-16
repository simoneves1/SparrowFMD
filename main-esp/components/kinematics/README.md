# kinematics

Cartesian trajectory planning: turns queued moves (position/velocity/
acceleration over time) into individual stepper pulse times. This is
main-esp acting as what Klipper calls the "host" -- the real Klipper
architecture always does this planning on the host (normally a Pi
running `klippy`), not on the MCU boards, which just execute
pre-computed step timings. That's why this exists here rather than
being left to whatever third-party board main-esp talks to.

## Provenance
`trapq.c/h`, `itersolve.c/h`, `kin_cartesian.c` are ported from Klipper's
own `klippy/chelper/` (`https://github.com/Klipper3d/klipper`, GPLv3),
originally fetched for the float32 kinematics prototype documented in
the root README's Status section and `git log`. `list.h`, `compiler.h`,
`pyhelper.h`, `src/stepcompress.h` are unmodified, vendored only because
the ported files `#include` them.

The port is mechanical `double` -> `float` (the P4 has no hardware
double-precision FPU) plus reworked convergence epsilons in
`itersolve.c` -- see that file's own comments for why a *fixed* epsilon
doesn't work (either unrepresentable in float32 at realistic magnitudes,
or loose enough to cause duplicate-step artifacts) and how the adaptive
(relative-to-half_step, floored by float32's own ULP at the operative
magnitude) replacement resolves it. That finding was validated by a
host-native sanity sweep across realistic bed sizes and print durations
before this code was promoted from a benchmark spike into this real
component -- see `test/test_main.c`.

## Status
Host-tested (this component builds and runs outside ESP-IDF, no
hardware needed -- see `test/test_main.c`). Cartesian only, matching
this project's stated scope (no CoreXY/delta support). NOT yet wired
into `gcode-parser` or any real step-transmission path -- there is no
gcode -> kinematics -> MCU pipeline yet, this is the trajectory-planning
piece of that pipeline built and verified on its own first.

`kinematics_steps.c/h` is this component's own minimal stand-in for
Klipper's real `stepcompress.c` (MCU step-queue run-length compression,
clock-tick conversion, position-history tracking for the wire protocol
-- none of that exists here). It only coalesces itersolve's repeated
same-step-refinement calls into one committed step event per physical
step and hands each to a caller-supplied callback -- the seam a future
klipper-host-protocol step-transmission piece plugs into. See that
header's top comment for why the coalescing exists at all (a real bug
found and fixed during the original prototype, not a stylistic choice).

## Not yet done
- Wiring to `gcode-parser`: turning a parsed G1/G0 command into a
  `trapq_append()` call doesn't exist yet.
- Wiring `kinematics_steps`' callback to `klipper-host-protocol`'s
  message encoding, to actually send steps to a real MCU board.
- Cross-compiled for esp32p4/esp32s3 as part of `main-esp`'s normal
  build (it compiles as a component, but nothing in `main.c` references
  it yet, so this hasn't been exercised on real/emulated hardware timing
  -- the original prototype's speed question is still open, see root
  README's Status section).
