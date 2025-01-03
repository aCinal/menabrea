cmake_minimum_required(VERSION 3.16)

project(menabrea_app)

# Propagate architecture definitions to all applications
add_compile_definitions(PRIVATE EM_64_BIT)
add_compile_definitions(PRIVATE ENV_64_BIT)

# Reduce shared objects sizes by only exporting the init/exit APIs
add_compile_options(-fvisibility=hidden)

add_subdirectory(pipeline)
add_subdirectory(echo)
add_subdirectory(dummy_test)
