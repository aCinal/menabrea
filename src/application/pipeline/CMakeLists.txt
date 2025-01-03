add_library(pipeline
    SHARED
    ingress.cc
    main.cc
    man_in_the_middle.cc
    pipeline.cc
    signer.cc
    verifier.cc)

# Yocto requires versioned libraries
set_target_properties(pipeline
    PROPERTIES
    VERSION 1.0.0)

install(TARGETS pipeline RUNTIME)
