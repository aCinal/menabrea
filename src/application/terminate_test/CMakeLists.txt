add_library(terminate
    SHARED
    terminate_test.cc)

# Yocto requires versioned libraries
set_target_properties(terminate
    PROPERTIES
    VERSION 1.0.0)

install (TARGETS terminate RUNTIME)
