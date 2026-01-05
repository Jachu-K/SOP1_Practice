cmake_minimum_required(VERSION 4.0)
project(lab3 C)

set(CMAKE_C_STANDARD 11)

# Tworzenie targetu
add_executable(lab3 main.c)  # lub lista plików

# Ustawienia specyficzne dla targetu
target_compile_options(lab3 PRIVATE
        -std=gnu99
        -Wall
        -fsanitize=address,undefined
)

target_link_options(lab3 PRIVATE
        -fsanitize=address,undefined
)

target_link_libraries(lab3 PRIVATE
        pthread
        m
)