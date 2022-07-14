add_library(msgbuffer
    SHARED
    message_buffering_test.cc)

# Yocto requires versioned libraries
set_target_properties(msgbuffer
    PROPERTIES
    VERSION 1.0.0)

install (TARGETS msgbuffer RUNTIME)
