add_library(periodictest
    SHARED
    periodic_timer_test.cc)

# Yocto requires versioned libraries
set_target_properties(periodictest
    PROPERTIES
    VERSION 1.0.0)

install (TARGETS periodictest RUNTIME)
