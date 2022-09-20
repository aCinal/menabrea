add_library(echo
    SHARED
    echo.cc)

# Yocto requires versioned libraries
set_target_properties(echo
    PROPERTIES
    VERSION 1.0.0)

install (TARGETS echo RUNTIME)
