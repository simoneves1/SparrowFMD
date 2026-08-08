idf_component_register(
    SRCS
        "src/vlq.c"
        "src/msgblock.c"
        "src/identify.c"
        "src/dictionary.c"
        "src/msgtable.c"
        "third_party/puff/puff.c"
    INCLUDE_DIRS
        "include"
    REQUIRES
        json
)

# puff.c is vendored verbatim (see third_party/puff/README.md) -- not
# ours to fix warnings in.
set_source_files_properties(third_party/puff/puff.c PROPERTIES
    COMPILE_OPTIONS "-Wno-unused-but-set-variable"
)
