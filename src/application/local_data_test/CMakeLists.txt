add_library(localdata
    SHARED
    local_data_test.cc)

# Yocto requires versioned libraries
set_target_properties(localdata
    PROPERTIES
    VERSION 1.0.0)

install (TARGETS localdata RUNTIME)
