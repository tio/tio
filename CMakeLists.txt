cmake_minimum_required(VERSION 3.17)
project(adaptiveTerminal)

set(CMAKE_CXX_STANDARD 20)

include_directories(
        src/include
        src/include/tio
)

add_library(tio
        src/error.c
        src/log.c
        src/options.c
        src/time.c
        src/tty.c
)