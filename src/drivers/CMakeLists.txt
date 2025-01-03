cmake_minimum_required(VERSION 3.16)
project(menabrea_drivers_user)

# Turn all warnings into errors
add_compile_options(-Wall -Werror)

include_directories(kernel/include)
add_subdirectory(user)
