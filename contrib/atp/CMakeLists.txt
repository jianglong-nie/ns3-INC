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
    model/atp-bulk-send-application.cc
    helper/atp-bulk-send-helper.cc

    model/atp-header.cc
    model/atp-tag.cc

    model/atp-l4-protocol.cc
    model/atp-tx-buffer.cc
    model/atp-socket.cc
    model/atp-socket-factory.cc

    model/atp-packet-sink.cc
    helper/atp-packet-sink-helper.cc

    model/atp-aggregator.cc

    model/atp-static-routing.cc
    helper/atp-static-routing-helper.cc
)

set(header_files
    model/atp-bulk-send-application.h
    helper/atp-bulk-send-helper.h

    model/atp-header.h
    model/atp-tag.h

    model/atp-l4-protocol.h
    model/atp-tx-buffer.h
    model/atp-socket.h
    model/atp-socket-factory.h

    model/atp-packet-sink.h
    helper/atp-packet-sink-helper.h

    model/atp-aggregator.h

    model/atp-static-routing.h
    helper/atp-static-routing-helper.h
)
build_lib(
    LIBNAME atp
    SOURCE_FILES ${source_files}
    HEADER_FILES ${header_files}
    LIBRARIES_TO_LINK ${libinternet}
                      ${libapplications}
)