cmake_minimum_required(VERSION 3.28.3)
project(Hello3 C)

set(CMAKE_C_STANDARD 11)

add_executable(Hello3
        src/main.c
        display.c
        display.h
        display.c
        display.c
        include/config.h
        include/game.h
        src/game.c)
