cmake_minimum_required(VERSION 3.15)
project(dildobean C)

set(CMAKE_C_STANDARD 11)

set(SOURCES
        src/main.c
        src/adapt.c
        src/queue.c
        src/backend/ffmpeg.c
)

add_executable(dildobean ${SOURCES})

if(WIN32)
    target_link_options(dildobean PRIVATE -mconsole)
endif()
