idf_component_register(
    SRCS
        "src/vlq.c"
        "src/msgblock.c"
        "src/identify.c"
        "src/dictionary.c"
        "src/msgtable.c"
        "src/message.c"
        "src/session.c"
        "src/uart_transport.c"
        "src/usb_cdc_transport.c"
        "third_party/puff/puff.c"
    INCLUDE_DIRS
        "include"
    REQUIRES
        json
        esp_driver_uart
        usb
        usb_host_cdc_acm
)

# puff.c is vendored verbatim (see third_party/puff/README.md) -- not
# ours to fix warnings in.
set_source_files_properties(third_party/puff/puff.c PROPERTIES
    COMPILE_OPTIONS "-Wno-unused-but-set-variable"
)
