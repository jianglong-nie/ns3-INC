build_lib_example(
    NAME atp-p2p
    SOURCE_FILES atp-p2p.cc
    LIBRARIES_TO_LINK ${libatp}
                      ${libinternet}
                      ${libapplications}
                      ${libpoint-to-point}
)

build_lib_example(
    NAME atp-single-job
    SOURCE_FILES atp-single-job.cc
    LIBRARIES_TO_LINK ${libatp}
                      ${libinternet}
                      ${libapplications}
                      ${libcsma}
                      ${libbridge}
)

build_lib_example(
    NAME atp-multi-jobs
    SOURCE_FILES atp-multi-jobs.cc
    LIBRARIES_TO_LINK ${libatp}
                      ${libinternet}
                      ${libapplications}
                      ${libcsma}
                      ${libbridge}
)

build_lib_example(
    NAME atp-throughput
    SOURCE_FILES atp-throughput.cc
    LIBRARIES_TO_LINK ${libatp}
                      ${libinternet}
                      ${libapplications}
                      ${libcsma}
                      ${libbridge}
)
