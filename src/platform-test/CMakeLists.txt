cmake_minimum_required(VERSION 3.16)
project(menabrea_platform_test)

# Turn all warnings into errors
add_compile_options(-Wall -Werror)
# Reduce shared objects sizes by only exporting the init/exit APIs
add_compile_options(-fvisibility=hidden)

# Add architecture definitions
add_compile_definitions(PRIVATE EM_64_BIT)
add_compile_definitions(PRIVATE ENV_64_BIT)

add_library(regression SHARED main.cc)
# Yocto requires versioned libraries
set_target_properties(regression
    PROPERTIES
    VERSION 1.0.0)

include_directories("../platform/interface")
include_directories(.)

add_subdirectory(cases)
add_subdirectory(framework)
add_subdirectory(scripts)

target_link_libraries(regression
    cases
    framework
)

install (TARGETS regression RUNTIME)
