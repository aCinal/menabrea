cmake_minimum_required(VERSION 3.16)
project(menabrea_platform_test)

# Turn all warnings into errors
add_compile_options(-Wall -Werror)
# Reduce shared objects sizes by only exporting the init/exit APIs
add_compile_options(-fvisibility=hidden)

# Add architecture definitions
add_compile_definitions(PRIVATE EM_64_BIT)
add_compile_definitions(PRIVATE ENV_64_BIT)

add_library(platformtest SHARED main.cc)
# Yocto requires versioned libraries
set_target_properties(platformtest
    PROPERTIES
    VERSION 1.0.0)

include_directories(.)

add_subdirectory(cases)
add_subdirectory(scripts)

target_link_libraries(platformtest
    cases
)

install(TARGETS platformtest RUNTIME)
