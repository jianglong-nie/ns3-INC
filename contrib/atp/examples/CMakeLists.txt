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

build_lib_example(
    NAME atp-p2p-static-routing
    SOURCE_FILES atp-p2p-static-routing.cc
    LIBRARIES_TO_LINK ${libatp}
                      ${libinternet}
                      ${libapplications}
                      ${libpoint-to-point}
)

build_lib_example(
    NAME atp-p2p-atp-static-routing
    SOURCE_FILES atp-p2p-atp-static-routing.cc
    LIBRARIES_TO_LINK ${libatp}
                      ${libinternet}
                      ${libapplications}
                      ${libpoint-to-point}
)

build_lib_example(
    NAME atp-p2p-atp-static-routing-multi-jobs
    SOURCE_FILES atp-p2p-atp-static-routing-multi-jobs.cc
    LIBRARIES_TO_LINK ${libatp}
                      ${libinternet}
                      ${libapplications}
                      ${libpoint-to-point}
)

build_lib_example(
    NAME atp-p2p-ecn-test
    SOURCE_FILES atp-p2p-ecn-test.cc
    LIBRARIES_TO_LINK ${libatp}
                      ${libinternet}
                      ${libapplications}
                      ${libpoint-to-point}
)

build_lib_example(
    NAME atp-p2p-2w5w
    SOURCE_FILES atp-p2p-2w5w.cc
    LIBRARIES_TO_LINK ${libatp}
                      ${libinternet}
                      ${libapplications}
                      ${libpoint-to-point}
)

build_lib_example(
    NAME atp-p2p-2w5w-diffRTT
    SOURCE_FILES atp-p2p-2w5w-diffRTT.cc
    LIBRARIES_TO_LINK ${libatp}
                      ${libinternet}
                      ${libapplications}
                      ${libpoint-to-point}
)