
# Allow components access to internal interfaces
# TODO: Define stricter dependencies
include_directories(exception)
include_directories(log)
include_directories(startup)
include_directories(timing)
include_directories(work)

# Propagate architecture definitions to all components
add_compile_definitions(PRIVATE EM_64_BIT)
add_compile_definitions(PRIVATE ENV_64_BIT)

add_subdirectory(exception)
add_subdirectory(log)
add_subdirectory(networking)
add_subdirectory(startup)
add_subdirectory(timing)
add_subdirectory(work)

target_link_libraries(menabrea
    # Link against platform components
    exception
    log
    networking
    startup
    timing
    work
    # Link against EM-ODP
    emodp
    odp-linux
    # Link against standard libraries
    dl)

# Export global symbols for use by dynamically linked libraries
set_property(TARGET menabrea PROPERTY ENABLE_EXPORTS ON)
