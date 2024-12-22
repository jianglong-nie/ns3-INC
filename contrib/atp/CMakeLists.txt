check_include_file_cxx(stdint.h HAVE_STDINT_H)
if(HAVE_STDINT_H)
    add_definitions(-DHAVE_STDINT_H)
endif()

set(examples_as_tests_sources)
if(${ENABLE_EXAMPLES})
    set(examples_as_tests_sources
        #test/atp-examples-test-suite.cc
        )
endif()

set(source_files
    model/atp.cc
    helper/atp-helper.cc

    model/atp-bulk-send-application.cc
    helper/atp-bulk-send-helper.cc

    model/atp-header.cc
    model/atp-tag.cc
)

set(header_files
    model/atp.h
    helper/atp-helper.h

    model/atp-bulk-send-application.h
    helper/atp-bulk-send-helper.h

    model/atp-header.h
    model/atp-tag.h
)

build_lib(
    LIBNAME atp
    SOURCE_FILES ${source_files}
    HEADER_FILES ${header_files}
    LIBRARIES_TO_LINK ${libinternet}
                      ${libapplications}
                      ${libcsma}
                      ${libbridge}
    TEST_SOURCES test/atp-test-suite.cc
                 ${examples_as_tests_sources}
)