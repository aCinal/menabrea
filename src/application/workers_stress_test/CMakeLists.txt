add_library(workstress
    SHARED
    workers_stress_test.cc)

# Yocto requires versioned libraries
set_target_properties(workstress
    PROPERTIES
    VERSION 1.0.0)

install (TARGETS workstress RUNTIME)
