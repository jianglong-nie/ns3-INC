

build_lib_example(
    NAME atp-v1-1job
    SOURCE_FILES atp-v1-1job.cc
    LIBRARIES_TO_LINK ${libatp}
                      ${libinternet}
                      ${libapplications}
                      ${libpoint-to-point}
)

build_lib_example(
    NAME atp-v1-2jobs
    SOURCE_FILES atp-v1-2jobs.cc
    LIBRARIES_TO_LINK ${libatp}
                      ${libinternet}
                      ${libapplications}
                      ${libpoint-to-point}
)

build_lib_example(
    NAME atp-v1-newtopo
    SOURCE_FILES atp-v1-newtopo.cc
    LIBRARIES_TO_LINK ${libatp}
                      ${libinternet}
                      ${libapplications}
                      ${libpoint-to-point}
)

build_lib_example(
    NAME atp-v1-newtopo-1job
    SOURCE_FILES atp-v1-newtopo-1job.cc
    LIBRARIES_TO_LINK ${libatp}
                      ${libinternet}
                      ${libapplications}
                      ${libpoint-to-point}
)