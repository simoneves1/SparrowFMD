# cJSON

Vendored verbatim from this machine's ESP-IDF installation
(`components/json/cJSON/`), which in turn vendors
[DaveGamble/cJSON](https://github.com/DaveGamble/cJSON) upstream.

**License**: MIT (see the header comment in `cJSON.h`) --
GPLv3-compatible.

**Only used for host-buildable unit tests.** The real esp32p4 firmware
build does not compile this copy -- `khp_msgtable.c` requires
ESP-IDF's own `json` component instead (`REQUIRES json` in this
component's `CMakeLists.txt`), so there's exactly one copy of cJSON
linked into the real firmware, not two. This directory exists purely so
`test/test_main.c` can compile and run outside of ESP-IDF, the same way
`third_party/puff/` does.

Do not modify this file -- if a fix or upstream update is ever needed,
re-vendor from ESP-IDF (or upstream cJSON) rather than hand-patching.
