cmake_minimum_required(VERSION 3.16)
project(menabrea)

# Turn all warnings into errors
add_compile_options(-Wall -Werror)

add_executable(menabrea)

include_directories(interface)
add_subdirectory(components)
add_subdirectory(scripts)

install(TARGETS menabrea RUNTIME)

# Install the headers and retain the menabrea namespace so
# that platform and applications have the same view of include
# paths. This installs the headers in /usr/include in the sysroot
# which is then visible to dependent recipes, e.g., applications.
install(DIRECTORY interface/ DESTINATION include)
# TODO: Use CMake's target_sources to install public headers
