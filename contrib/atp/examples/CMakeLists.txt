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
