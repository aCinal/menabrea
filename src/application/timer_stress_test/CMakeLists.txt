add_library(timerstress
    SHARED
    timer_stress_test.cc)

# Yocto requires versioned libraries
set_target_properties(timerstress
    PROPERTIES
    VERSION 1.0.0)

install (TARGETS timerstress RUNTIME)

# Use C++17 to have structured bindings
set_property(
    TARGET timerstress
    PROPERTY CXX_STANDARD 17
)
