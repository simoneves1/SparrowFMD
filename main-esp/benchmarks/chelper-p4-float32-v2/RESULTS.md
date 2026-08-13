# float32 chelper prototype v2 — adaptive epsilon retry

## Recap
The first float32 prototype (see root README's Status section for its
summary; its own code/RESULTS.md were deleted per this project's spike
convention) found a real correctness blocker: `itersolve.c`'s
double-precision-calibrated fixed epsilons (`1e-9`) are unrepresentable in
float32 at realistic magnitudes, but loosening them to a fixed,
representable value (`1e-4`) caused ~80% step-count inflation (duplicate
step commands), because that epsilon wasn't tight enough relative to a
real microstepping `half_step` distance. Verdict was inconclusive/negative,
with an explicitly proposed but unattempted fix: a relative/adaptive
epsilon instead of one fixed constant.

## This attempt
`itersolve.c`'s `dist_eps()`/`time_eps()` now compute the epsilon per
comparison as the larger of two things: a tolerance kept relative to the
physically meaningful scale in play (`half_step * 0.01` for distance), and
float32's own representable-ULP floor at the current magnitude of the
value being compared (`8 * FLT_EPSILON * fabsf(magnitude)`) — standard
relative/absolute-floor epsilon construction, not a single guessed
constant.

## Verdict: works cleanly at realistic magnitudes. The earlier
## correctness blocker is resolved.

A host-native sanity harness (accel/cruise/decel X move, 200 steps/mm,
compared against a coalescing-corrected double-precision control — see
git history for how that stub bug was found and fixed) swept two
independent magnitude axes:

**Position magnitude** (0mm to 1000mm start position, fixed print_time
~1hr in):

| start position | step count | vs. expected | min step delta vs. expected |
|---|---|---|---|
| 0mm | 12004 | +0.03% | -1.7e-7s |
| 120mm | 11989 | -0.09% | -8.7e-7s |
| 250mm (typical bed) | 11989 | -0.09% | -8.6e-7s |
| 500mm (large-format bed) | 11989 | -0.09% | -1.9e-6s |
| 1000mm (unrealistic gantry) | 12169 | **+1.41%** | -1.6e-5s |

Step count stays within ~0.1% of the analytically-expected 12000 across
every realistic bed size (0–500mm) — a world away from the first
attempt's 80% inflation. It only starts visibly degrading at 1000mm, well
past any bed size this project would ever build. `nonmono` (steps
recorded out of time order) stayed at 1–2 out of ~12000 in every case,
identical to what the *double-precision* control showed in the first
prototype's investigation — an existing harness/algorithm property, not a
float32 regression (not root-caused further, doesn't change the verdict).

**print_time magnitude** (17s to 259217s / ~72 hours, fixed 250mm
position): relative step-to-step spacing came back **bit-identical**
(`0.000049138s` min delta, to 9 decimal places) at every offset tested,
from the start of a print to 72 hours in. This confirms what the first
prototype's data already suggested: `itersolve_gen_steps_range` rebases
to move-local time before doing any epsilon-sensitive math, so the
iterative solver's convergence and the resulting relative step timing
are architecturally insulated from how large absolute print_time has
grown — only the *absolute* step timestamp (reconstructed as
`m->print_time + local_time`) carries the already-known, unavoidable
float32 storage error (0.6µs at t=17s, growing to ~192µs by 8+ hours in,
consistent with float32's ULP scaling). That absolute-timestamp error is
a separate, bounded, real property of storing `print_time` as float32 —
not something an epsilon can fix, and not shown here to be a blocker for
motion smoothness, since relative spacing (what actually drives step
pulses) doesn't inherit it.

## What this means for main-esp
The correctness blocker that stopped the first prototype is resolved: an
adaptive epsilon construction (relative to `half_step`, floored by
float32's ULP at the operative magnitude) produces step output that
matches the expected physical step count and timing to within a fraction
of a percent, across every bed size and print duration this project would
realistically hit. The open question this was always meant to answer --
does chelper run fast enough on the P4's hardware float32 FPU -- is now
back to being purely a speed question, not a correctness one. That speed
question (cross-compile + real/emulated timing) has not been attempted in
this round; RESULTS.md is being written now as a decision point before
committing to that next, larger step.
