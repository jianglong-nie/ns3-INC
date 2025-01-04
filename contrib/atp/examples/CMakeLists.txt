build_lib_example(
    NAME atp-example
    SOURCE_FILES atp-example.cc
    LIBRARIES_TO_LINK ${libatp}
)

build_lib_example(
    NAME atp-bulk-send
    SOURCE_FILES atp-bulk-send.cc
    LIBRARIES_TO_LINK ${libatp}
                      ${libinternet}
                      ${libapplications}
                      ${libpoint-to-point}                      
)

build_lib_example(
    NAME atp-bulk-header
    SOURCE_FILES atp-bulk-header.cc
    LIBRARIES_TO_LINK ${libatp}
                      ${libinternet}
                      ${libapplications}
                      ${libcsma}
                      ${libbridge}
)


build_lib_example(
    NAME atp-bulk-tag
    SOURCE_FILES atp-bulk-tag.cc
    LIBRARIES_TO_LINK ${libatp}
                      ${libinternet}
                      ${libapplications}
                      ${libcsma}
                      ${libbridge}
)

build_lib_example(
    NAME atp-bridge
    SOURCE_FILES atp-bridge.cc
    LIBRARIES_TO_LINK ${libatp}
                      ${libinternet}
                      ${libapplications}
                      ${libcsma}
                      ${libbridge}
)

build_lib_example(
    NAME atp-bridge-cc
    SOURCE_FILES atp-bridge-cc.cc
    LIBRARIES_TO_LINK ${libatp}
                      ${libinternet}
                      ${libapplications}
                      ${libcsma}
                      ${libbridge}
)

build_lib_example(
    NAME atp-tcp-cc
    SOURCE_FILES atp-tcp-cc.cc
    LIBRARIES_TO_LINK ${libatp}
                      ${libinternet}
                      ${libapplications}
                      ${libcsma}
                      ${libbridge}
)