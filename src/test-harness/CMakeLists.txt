cmake_minimum_required(VERSION 3.16)
project(menabrea_test_harness)

# Turn all warnings into errors
add_compile_options(-Wall -Werror)

# Add architecture definitions
add_compile_definitions(PRIVATE EM_64_BIT)
add_compile_definitions(PRIVATE ENV_64_BIT)

include_directories(lib)
include_directories(lib/interface)

add_subdirectory(app)
add_subdirectory(lib)
add_subdirectory(scripts)
