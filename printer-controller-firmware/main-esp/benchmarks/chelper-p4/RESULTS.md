# chelper-on-P4 kinematics benchmark — results

## Verdict

**At risk / inconclusive, leaning "does not fit as-is."** Confidence:
**low-medium** (structural finding, not a hardware measurement — see
Confidence section).

The single concrete, high-confidence finding from this spike: **the
ESP32-P4's RISC-V core has no double-precision hardware FPU**
(`-march=rv32imafc...` — the `f` extension is single-precision only, there
is no `d`). Klipper's chelper does all trajectory math in C `double`.
Cross-compiling the real, unmodified chelper source for `esp32p4` and
disassembling the result confirms every add/sub/mul/div/compare on a
`double` in the hot path becomes a **call to a software floating-point
routine** (`__adddf3`, `__subdf3`, `__muldf3`, `__divdf3`, `__ltdf2`,
`__gtdf2`, ...) rather than a single hardware instruction:

| Function (hot path) | soft-double calls | total instructions |
|---|---|---|
| `itersolve_gen_steps_range` (innermost per-step solver loop) | 48 | 597 |
| `itersolve_generate_steps` | 15 | 235 |
| `trapq_append` | 7 | 251 |
| `move_get_coord` | 9 | 105 |
| `stepcompress_append` | 5 | 89 |

Each of those calls is itself a multi-instruction software IEEE-754
routine, so the real dynamic instruction count executed per step is
substantially higher than the static counts above suggest — the 8% of
*static* instructions in `itersolve_gen_steps_range` that are soft-double
calls likely account for the majority of *executed* cycles, since that
function's whole job is arithmetic on `double`s in a tight secant-method
loop (see `itersolve.c:28-128`).

This is the actual blocker, more specific than "is the P4 fast enough":
**chelper as-is asks a single-precision-only core to emulate
double-precision math in its innermost loop.** That's a structural
mismatch independent of clock speed.

## What was measured vs. estimated

No physical P4 hardware was available, and ESP-IDF v5.3.1's QEMU doesn't
support the `esp32p4` target at all. As a fallback, `esp32c3` — the only
QEMU-supported RISC-V target in this IDF version — was used as a proxy,
since it shares the base RISC-V ISA. Four things were actually done:

1. **Host-native sanity run** (`host_sanity/`, x86_64, real hardware
   FPU): confirms the harness and synthetic move generation are correct.
   - 4000 G-code-equivalent segments, 3 steppers (X/Y/Z)
   - `itersolve_generate_steps` phase (X+Y+Z): 0.0538s for 4000 segments,
     ~1,707,325 steps → **~31.7M steps/sec** combined across 3 axes on
     this desktop CPU (hardware double FPU, ~4GHz, superscalar/OoO)
2. **Real cross-compile for `esp32p4`**: the unmodified chelper C sources
   build cleanly against ESP-IDF v5.3.1 for the actual P4 target
   (`idf.py build` succeeds, produces a flashable `.bin`).
3. **P4 QEMU: not available.** `idf.py qemu` explicitly rejects
   `esp32p4` ("QEMU is not supported for target esp32p4"). Confirmed by
   trying, not assumed.
4. **`esp32c3` QEMU run (RISC-V proxy, real code execution)**: the same
   chelper code, cross-compiled for `esp32c3` and run under QEMU,
   executed to completion and printed real results — 30 segments,
   ~12,189 approximate steps, `itersolve_generate_steps` phase 0.0556s
   (~219,000 steps/sec). This is real execution of the actual algorithm
   on a real (emulated) RISC-V core with **no hardware FPU at all**
   (`rv32imc`, not even single-precision) — a strictly harder case than
   the P4's `rv32imafc`.

   **This number should not be read as a hardware-speed measurement,
   though — see the caveat below.** It only confirms the algorithm
   executes correctly end-to-end on real RISC-V machine code, soft-double
   emulation included.

### Why the QEMU number isn't a timing measurement

ESP-IDF's QEMU integration (`idf_py_actions/qemu_ext.py`) invokes
`qemu-system-riscv32` without `-icount` (instruction-count-locked
timing) — confirmed by inspecting the actual command line QEMU was
launched with. Without `-icount`, QEMU's emulated timer peripheral
(which backs `esp_timer_get_time()`, used for all the timing in this
harness) advances against the **host machine's real wall-clock time** as
QEMU's JIT (TCG) translates and executes instructions — not against a
virtual clock scaled to represent genuine 160MHz silicon. A modern
desktop host JIT-translating simple RISC-V instructions can easily run
faster *or* slower than real embedded silicon at that clock rate,
depending on translation overhead per instruction; there's no fixed,
known ratio. So "0.0556s" measures "how long this specific host took to
JIT-emulate this code" — not any calibrated multiple of C3 or P4 hardware
time. Treat the QEMU run as a **correctness proof**, not a benchmark.

What was **not** done, and is the actual remaining gap: a real on-target
(or genuinely cycle-accurate simulated) timing run. The soft-double
finding above is strong structural evidence; the QEMU run adds
"executes correctly under real RISC-V soft-double emulation" as a second
independent confirmation; neither converts into a trustworthy steps/sec
number for the P4. That still requires either real hardware or a
cycle-accurate simulator (`-icount` mode might get closer but wasn't
attempted here, given the throughput number would still be for the
wrong chip — `esp32c3`, not `esp32p4`).

## Memory budget finding (separate from the CPU question)

Getting the `esp32c3` QEMU run working surfaced a real, separate issue:
the initial attempt at 4000 segments (matching the host run) and a
reduced 500-segment attempt both crashed with a NULL-pointer store fault
inside `move_alloc`'s `memset` — `malloc()` was silently returning NULL.
Root cause, confirmed via a plain `malloc(88)` test (worked fine, 330KB
free) and step-by-step isolation of the harness: **this benchmark
harness never calls `stepcompress_flush()`**, so each axis's internal
step-compression queue (`stepcompress.c`'s `queue` array) grows to hold
every step generated for the *entire* run, un-bounded, rather than being
periodically flushed and shrunk the way a real Klipper host does
continuously during a print. At even a few hundred G-code segments, the
real *step* count (not move count) reaches the hundreds of thousands,
and that queue growth alone exceeds the `esp32c3`'s ~330KB heap.

This isn't a finding about the P4 specifically (which has much more
RAM), but it's a real, actionable note for `main-esp`'s eventual
`kinematics` implementation: **it must flush and prune consumed moves
and step-compression state continuously** (as real Klipper's host does),
not accumulate a whole print's worth of state — this benchmark harness
took the simple "build everything, solve once" shortcut, and that
shortcut is specifically what doesn't scale to constrained memory. The
final 30-segment/C3 run avoids this by staying small, not by fixing the
harness to prune — a real implementation needs the pruning, this
benchmark just needed to dodge the issue to get a number.

## Throughput requirement (for context)

A representative worst case: 150mm/s cruise speed, 0.0125mm/step (16
microstepping, GT2 2mm pitch, 20T pulley) → 12,000 steps/sec on one axis.
Across X+Y+Z+E that's roughly 30,000-50,000 step-generation calls/sec
needed in the worst case (not all axes peak simultaneously in practice,
but this is the ballpark main-esp's kinematics module needs to sustain).
Host does ~42.7M steps/sec combined — ~1000x the requirement, unsurprising
for a desktop CPU with hardware double FPU. The open question is how much
of that 1000x margin survives a ~10x clock deficit plus an unquantified
soft-double emulation penalty on the P4.

## Recommendation

Two concrete paths forward, both more actionable than "wait for
hardware":

1. **Port chelper's hot path to `float` (single precision).** The P4
   *does* have a hardware single-precision FPU (`rv32imafc`). Klipper's
   own step-timing precision requirements are in the microsecond range;
   whether `float`'s ~7 decimal digits of precision is sufficient over a
   multi-hour print's accumulated print_time needs checking (this is
   exactly the kind of question a hardware run would also need to
   answer), but this is a scoped, testable change to make on this same
   harness before ruling out full Klipper-grade kinematics.
2. **Get real timing data**: either acquire P4 hardware (cheapest,
   fastest path to a real answer) or find/build a cycle-accurate RISC-V
   simulator that models the P4's actual pipeline (QEMU's instruction-count
   emulation, even if P4 support existed, doesn't model cycles accurately
   either — it wasn't going to fully answer this even if available).

Given the soft-double finding, **the Marlin-style local-execution
fallback described in `main-esp/README.md` should be treated as the more
likely direction** unless option 1 (float32 port) closes the gap —
recommend prototyping that port next rather than assuming full
Klipper-grade kinematics will fit.

## Confidence

**Low-medium.** High confidence in the soft-double-precision finding
itself (directly observed in real disassembly of a real cross-compile for
the real target, not inferred). Low confidence in any specific
steps/sec number for the P4, because none was actually measured — no
hardware, no working emulator. Treat the verdict as "here is a concrete
reason for concern, worth resolving before committing to the full
Klipper-grade design," not as a final no.

## Constraints this spike ran under

- No physical ESP32-P4 hardware available.
- ESP-IDF v5.3.1 QEMU does not support the `esp32p4` target (confirmed by
  running `idf.py qemu`, not assumed from docs); `esp32c3` was used as a
  RISC-V proxy instead, but its QEMU timing isn't calibrated to real
  silicon speed either (see above) and its `rv32imc` core has no FPU at
  all, unlike the P4's `rv32imafc`.
- No real sliced G-code file was available for the move sequence; the
  synthetic sequence in `host_sanity/harness_main.c` / `main/harness_main.c`
  was generated from typical FDM print parameters (zigzag infill +
  periodic layer lifts, 2500mm/s² accel, 20-120mm/s cruise speeds) rather
  than pulled from an actual print. This affects the *move-rate* numbers;
  it does not affect the soft-double-precision finding, which comes from
  the target's ISA, not the input data.
- The `esp32c3` QEMU run used only 30 segments (`NUM_SEGMENTS=30` in
  `main/CMakeLists.txt`), far short of the 4000 used elsewhere, because
  of the memory-budget issue above — small-N results are noisier and
  don't necessarily represent steady-state behavior.

## How to reproduce / extend

- Host sanity build: see `host_sanity/` — compile with any C compiler
  and the `msgblock.c`/`trapq.c`/`itersolve.c`/`kin_cartesian.c`/
  `stepcompress.c` files from `vendor/klipper/klippy/chelper/`, plus
  `harness_pyhelper.c` in place of the real `pyhelper.c` (that file's
  Python/glibc-specific bits don't apply here).
- P4 cross-compile: `idf.py build` from this directory (after
  `idf.py set-target esp32p4`). Flash to real hardware with `idf.py
  flash monitor` once a board is available — the harness prints its
  results over the serial console via `app_main()` in `main/harness_main.c`.
- Disassembly used for the soft-double finding:
  `riscv32-esp-elf-objdump -d build/chelper_p4_bench.elf`, then grep for
  `jal`/`jalr` targets landing on `__*df2`/`__*df3` symbols within
  `itersolve_gen_steps_range` and friends.
