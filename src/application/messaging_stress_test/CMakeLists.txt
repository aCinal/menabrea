add_library(msgstress
    SHARED
    messaging_stress_test.cc)

# Yocto requires versioned libraries
set_target_properties(msgstress
    PROPERTIES
    VERSION 1.0.0)

install (TARGETS msgstress RUNTIME)
