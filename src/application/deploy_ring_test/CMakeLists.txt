add_library(deployring
    SHARED
    deploy_ring_test.cc)

# Yocto requires versioned libraries
set_target_properties(deployring
    PROPERTIES
    VERSION 1.0.0)

install (TARGETS deployring RUNTIME)
