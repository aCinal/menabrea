add_library(dummytest
    SHARED
    dummy_test.cc)

# Yocto requires versioned libraries
set_target_properties(dummytest
    PROPERTIES
    VERSION 1.0.0)

install(TARGETS dummytest RUNTIME)
install(FILES   dummy_test_suite.json DESTINATION /opt)
