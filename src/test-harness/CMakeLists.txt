cmake_minimum_required(VERSION 3.16)
project(menabrea_test_harness)

# Turn all warnings into errors
add_compile_options(-Wall -Werror)
# Export all symbols, our APIs shall be called by test case implementations
add_compile_options(-fvisibility=default)
# TODO: Specify public functions and make only them visible

# Add architecture definitions
add_compile_definitions(PRIVATE EM_64_BIT)
add_compile_definitions(PRIVATE ENV_64_BIT)

set(SOURCES
    main.cc
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

include_directories(.)
include_directories(interface)

add_subdirectory(scripts)

install(TARGETS testharness RUNTIME)
install(DIRECTORY interface/ DESTINATION include)
