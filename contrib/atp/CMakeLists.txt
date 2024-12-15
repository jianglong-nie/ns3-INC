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
)

set(header_files
    model/atp.h
    helper/atp-helper.h 
)

build_lib(
    LIBNAME atp
    SOURCE_FILES ${source_files}
    HEADER_FILES ${header_files}
    LIBRARIES_TO_LINK ${libcore}
    TEST_SOURCES test/atp-test-suite.cc
                 ${examples_as_tests_sources}
)