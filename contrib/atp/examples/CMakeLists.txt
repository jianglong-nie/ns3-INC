set(source_files
    atp-example.cc
)

build_lib_example(
    NAME atp-example
    SOURCE_FILES ${source_files}
    LIBRARIES_TO_LINK ${libatp}
)