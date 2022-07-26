cmake_minimum_required(VERSION 3.16)

project(menabrea_app)
include_directories("../platform/interface")

# Propagate architecture definitions to all applications
add_compile_definitions(PRIVATE EM_64_BIT)
add_compile_definitions(PRIVATE ENV_64_BIT)

add_subdirectory(messaging_stress_test)
add_subdirectory(local_data_test)
add_subdirectory(deploy_ring_test)
add_subdirectory(message_buffering_test)
add_subdirectory(terminate_test)
add_subdirectory(timer_test)
add_subdirectory(timer_stress_test)
