set(SOURCES
    main.cc
)

add_library(testport SHARED ${SOURCES})
# Yocto requires versioned libraries
set_target_properties(testport
    PROPERTIES
    VERSION 1.0.0)

install(TARGETS testport RUNTIME)
