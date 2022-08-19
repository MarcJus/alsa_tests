cmake_minimum_required(VERSION 3.22)
project(tests C)

set(CMAKE_C_STANDARD 99)

add_executable(tests main.c)
target_link_libraries(tests asound dl m)
