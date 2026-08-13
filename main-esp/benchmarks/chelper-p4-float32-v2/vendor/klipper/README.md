# Vendored Klipper chelper sources

Fetched from `https://github.com/Klipper3d/klipper` (`klippy/chelper/`),
GPLv3 licensed, for this float32 precision prototype (see this
directory's parent `RESULTS.md`). `trapq.c/h`, `itersolve.c/h`, and
`kin_cartesian.c` have been mechanically ported from `double` to `float`
and had their convergence epsilons reworked (see comments in
`itersolve.c`) -- everything else here (`list.h`, `compiler.h`,
`pyhelper.h`, `stepcompress.h`) is unmodified, vendored only because the
ported files `#include` them.
