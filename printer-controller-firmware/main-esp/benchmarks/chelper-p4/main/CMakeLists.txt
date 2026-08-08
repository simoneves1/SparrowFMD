set(CHELPER_DIR "${CMAKE_CURRENT_LIST_DIR}/../vendor/klipper/klippy/chelper")

idf_component_register(
    SRCS
        "harness_main.c"
        "harness_pyhelper.c"
        "${CHELPER_DIR}/trapq.c"
        "${CHELPER_DIR}/itersolve.c"
        "${CHELPER_DIR}/kin_cartesian.c"
        "${CHELPER_DIR}/stepcompress.c"
        "${CHELPER_DIR}/msgblock.c"
    INCLUDE_DIRS
        "."
        "${CHELPER_DIR}"
)

# The vendored chelper sources are written against glibc's 32-bit `int`
# assumption for int32_t/uint32_t; on riscv32 those typedef to `long`,
# which -Wformat (promoted to -Werror by ESP-IDF) flags in their debug
# errorf() calls. Not ours to fix -- silence the noise instead.
target_compile_options(${COMPONENT_LIB} PRIVATE
    -Wno-unused-parameter -Wno-format -Wno-format-extra-args)

# esp32c3 is only used as a QEMU-supported RISC-V proxy for the (QEMU
# -unsupported) P4 -- its much smaller heap (~330KB vs the P4's much
# larger RAM) can't fit the full un-pruned NUM_SEGMENTS move queue this
# harness builds. Shrink it for that target only; see RESULTS.md.
if(IDF_TARGET STREQUAL "esp32c3")
    target_compile_definitions(${COMPONENT_LIB} PRIVATE NUM_SEGMENTS=30)
endif()
