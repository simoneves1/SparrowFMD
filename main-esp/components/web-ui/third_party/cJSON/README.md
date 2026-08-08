# cJSON

Vendored verbatim from this machine's ESP-IDF installation
(`components/json/cJSON/`), which in turn vendors
[DaveGamble/cJSON](https://github.com/DaveGamble/cJSON) upstream.

**License**: MIT (see the header comment in `cJSON.h`) --
GPLv3-compatible.

**Only used for host-buildable unit tests.** The real esp32p4 firmware
build does not compile this copy -- `web_api.c` requires ESP-IDF's own
`json` component instead (`REQUIRES json` in this component's
`CMakeLists.txt`), so there's exactly one copy of cJSON linked into the
real firmware. Same pattern as
`klipper-host-protocol/third_party/cJSON/` -- see that copy's own
README for why each host-testable component vendors its own rather
than sharing one across components.

Do not modify this file -- re-vendor from ESP-IDF (or upstream cJSON)
rather than hand-patching.
