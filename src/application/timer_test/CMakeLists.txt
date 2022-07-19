add_library(timertest
    SHARED
    timer_test.cc)

# Yocto requires versioned libraries
set_target_properties(timertest
    PROPERTIES
    VERSION 1.0.0)

install (TARGETS timertest RUNTIME)
