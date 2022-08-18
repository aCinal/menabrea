cmake_minimum_required(VERSION 3.16)

project(menabrea_app)
include_directories("../platform/interface")

# Propagate architecture definitions to all applications
add_compile_definitions(PRIVATE EM_64_BIT)
add_compile_definitions(PRIVATE ENV_64_BIT)

add_subdirectory(deploy_ring_test)
add_subdirectory(message_buffering_test)
add_subdirectory(terminate_test)
add_subdirectory(timer_test)
add_subdirectory(timer_stress_test)
add_subdirectory(periodic_timer_test)
add_subdirectory(internode_test)
add_subdirectory(shared_memory_test)
