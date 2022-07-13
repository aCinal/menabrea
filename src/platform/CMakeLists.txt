cmake_minimum_required(VERSION 3.16)
project(menabrea)
add_executable(menabrea)

# Turn all warnings into errors
add_compile_options(-Wall -Werror)

include_directories(interface)
add_subdirectory(components)
add_subdirectory(scripts)

install (TARGETS menabrea RUNTIME)

# Install the headers and retain the menabrea namespace so
# that platform and applications have the same view of include
# paths
# TODO: Use CMake's target_sources to install public headers
install (DIRECTORY interface/ DESTINATION include)
