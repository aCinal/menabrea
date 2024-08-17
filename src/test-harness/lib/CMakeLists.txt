set(SOURCES
    ipc_socket.cc
    params_parser.cc
    test_case.cc
    test_runner.cc
)

add_library(testharness SHARED ${SOURCES})
# Yocto requires versioned libraries
set_target_properties(testharness
    PROPERTIES
    VERSION 1.0.0)

install(TARGETS testharness RUNTIME)
install(DIRECTORY interface/ DESTINATION include)
