# puff

Vendored verbatim from
[madler/zlib](https://github.com/madler/zlib)'s `contrib/puff`
(`puff.c`/`puff.h`, version 2.3, 21 Jan 2013), by Mark Adler.

A minimal, deliberately-simple reference DEFLATE decoder -- chosen over
zlib's own `inflate()` (much larger, tuned for speed) or ESP-IDF's
ROM-provided miniz (esp32p4-specific ROM support unconfirmed) because it
does exactly one thing, is small enough to read in full, and is
self-contained (no build-system integration beyond compiling one `.c`
file).

**License**: zlib license (see the header comment in `puff.h`) --
permissive, GPLv3-compatible.

**Scope**: raw DEFLATE only, no zlib/gzip container support. Klipper's
identify dictionary is zlib-wrapped (2-byte header + deflate stream +
4-byte Adler-32 trailer), so `khp_dictionary.c` strips the zlib header
itself before calling `puff()` on the remaining bytes.

Do not modify these two files -- if a fix or upstream update is ever
needed, re-vendor from upstream rather than hand-patching, so this stays
a clean, verifiable copy.
