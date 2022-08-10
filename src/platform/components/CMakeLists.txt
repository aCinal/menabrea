# Propagate architecture definitions to all components
add_compile_definitions(PRIVATE EM_64_BIT)
add_compile_definitions(PRIVATE ENV_64_BIT)

# Include current directory to allow accessing platform-internal interfaces
# but require fully qualified/namespaced names, i.e. <timing/timer_table.h>
# instead of <timer_table.h>
include_directories(.)

add_subdirectory(cores)
add_subdirectory(exception)
add_subdirectory(input)
add_subdirectory(log)
add_subdirectory(messaging)
add_subdirectory(startup)
add_subdirectory(timing)
add_subdirectory(work)

target_link_libraries(menabrea
    # Link against platform components
    cores
    exception
    log
    input
    messaging_common
    messaging_local
    messaging_network
    startup
    timing
    work
    # Link against EM-ODP
    emodp
    odp-linux
    # Link against standard libraries
    dl
)

# Export global symbols for use by dynamically linked libraries
set_property(TARGET menabrea PROPERTY ENABLE_EXPORTS ON)
