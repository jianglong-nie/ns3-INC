

build_lib_example(
    NAME atp-v1-1job
    SOURCE_FILES atp-v1-1job.cc
    LIBRARIES_TO_LINK ${libatp}
                      ${libinternet}
                      ${libapplications}
                      ${libpoint-to-point}
)