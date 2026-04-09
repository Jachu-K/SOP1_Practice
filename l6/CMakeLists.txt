cmake_minimum_required(VERSION 4.0)
project(l6z2 C)

set(CMAKE_C_STANDARD 11)

add_executable(l6z2 main.c)
target_link_libraries(l6z2 m)