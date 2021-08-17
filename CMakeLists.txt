cmake_minimum_required(VERSION 3.0)

project(df-read2)

set(CMAKE_CXX_FLAGS "--std=c++17 -Wall -pedantic")

set(df_read2_SOURCES
    src/df-read2.cpp
    src/inflate.c
    src/mblock.c
)

include_directories("include")

add_executable(df-read2 ${df_read2_SOURCES})

