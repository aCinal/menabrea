add_library(shmemtest
    SHARED
    shared_memory_test.cc)

# Yocto requires versioned libraries
set_target_properties(shmemtest
    PROPERTIES
    VERSION 1.0.0)

install (TARGETS shmemtest RUNTIME)
