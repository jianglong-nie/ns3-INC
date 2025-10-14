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

build_lib_example(
    NAME atp-p2p-2w5w-auto
    SOURCE_FILES atp-p2p-2w5w-auto.cc
    LIBRARIES_TO_LINK ${libatp}
                      ${libinternet}
                      ${libapplications}
                      ${libpoint-to-point}
)

build_lib_example(
    NAME atp-p2p-2w5w-water-filling
    SOURCE_FILES atp-p2p-2w5w-water-filling.cc
    LIBRARIES_TO_LINK ${libatp}
                      ${libinternet}
                      ${libapplications}
                      ${libpoint-to-point}
)

build_lib_example(
    NAME atp-p2p-2w2w-diffRTT
    SOURCE_FILES atp-p2p-2w2w-diffRTT.cc
    LIBRARIES_TO_LINK ${libatp}
                      ${libinternet}
                      ${libapplications}
                      ${libpoint-to-point}
)

build_lib_example(
    NAME atp-p2p-3job
    SOURCE_FILES atp-p2p-3job.cc
    LIBRARIES_TO_LINK ${libatp}
                      ${libinternet}
                      ${libapplications}
                      ${libpoint-to-point}
)

build_lib_example(
    NAME atp-p2p-4job
    SOURCE_FILES atp-p2p-4job.cc
    LIBRARIES_TO_LINK ${libatp}
                      ${libinternet}
                      ${libapplications}
                      ${libpoint-to-point}
)

build_lib_example(
    NAME atp-p2p-5job
    SOURCE_FILES atp-p2p-5job.cc
    LIBRARIES_TO_LINK ${libatp}
                      ${libinternet}
                      ${libapplications}
                      ${libpoint-to-point}
)

build_lib_example(
    NAME atp-p2p-2w2w-diffstarttime
    SOURCE_FILES atp-p2p-2w2w-diffstarttime.cc
    LIBRARIES_TO_LINK ${libatp}
                      ${libinternet}
                      ${libapplications}
                      ${libpoint-to-point}
)

build_lib_example(
    NAME atp-p2p-1w1w
    SOURCE_FILES atp-p2p-1w1w.cc
    LIBRARIES_TO_LINK ${libatp}
                      ${libinternet}
                      ${libapplications}
                      ${libpoint-to-point}
)

build_lib_example(
    NAME atp-p2p-1w1w1w1w
    SOURCE_FILES atp-p2p-1w1w1w1w.cc
    LIBRARIES_TO_LINK ${libatp}
                      ${libinternet}
                      ${libapplications}
                      ${libpoint-to-point}
)

build_lib_example(
    NAME atp-hierarchical-aggregation
    SOURCE_FILES atp-hierarchical-aggregation.cc
    LIBRARIES_TO_LINK ${libatp}
                      ${libinternet}
                      ${libapplications}
                      ${libpoint-to-point}
)

build_lib_example(
    NAME atp-hierarchical-aggregation-twojobs
    SOURCE_FILES atp-hierarchical-aggregation-twojobs.cc
    LIBRARIES_TO_LINK ${libatp}
                      ${libinternet}
                      ${libapplications}
                      ${libpoint-to-point}
)