add_library(mring SHARED mring.c)
# Yocto requires versioned libraries
set_target_properties(mring
    PROPERTIES
    VERSION 1.0.0)

install(TARGETS mring RUNTIME)
install(DIRECTORY include/ DESTINATION include)
