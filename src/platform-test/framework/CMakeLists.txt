set(SOURCES
    ipc_socket.cc
    params_parser.cc
    test_case.cc
    test_runner.cc
)

add_library(framework OBJECT ${SOURCES})
