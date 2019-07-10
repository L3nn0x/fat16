cmake_minimum_required(VERSION 3.14)

project(stoof)

add_subdirectory(kernel)
add_subdirectory(ls)

add_executable(stoof
    main.c
)

include_directories(kernel)

target_link_libraries(stoof kernel)
