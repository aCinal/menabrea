add_library(internode
    SHARED
    internode_test.cc)

# Yocto requires versioned libraries
set_target_properties(internode
    PROPERTIES
    VERSION 1.0.0)

install (TARGETS internode RUNTIME)
