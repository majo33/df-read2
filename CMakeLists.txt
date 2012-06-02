cmake_minimum_required(VERSION 2.8)

set(CMAKE_CXX_FLAGS "--std=c++0x -Wall -pedantic")

set(df_read2_SOURCES
    src/df-read2.cpp
    src/inflate.c
    src/mblock.c
)

find_package(Boost 1.42.0 REQUIRED COMPONENTS filesystem system)

include_directories("include")

add_executable(df-read2 ${df_read2_SOURCES})
target_link_libraries(df-read2 ${Boost_LIBRARIES})

