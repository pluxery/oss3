cmake_minimum_required(VERSION 3.27)
project(oss3)

set(CMAKE_CXX_STANDARD 17)

include_directories(.)

add_executable(oss3
        main.cpp
        Program.hpp
        SharedMemory.hpp
        TimeUtils.hpp)
