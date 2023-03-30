cmake_minimum_required(VERSION 3.24)
project(hllr)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE) # enables LTO

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread -flto -march=native -mtune=native")

add_executable(hllr main.cpp gc.cpp)

add_executable(alloc_min alloc_min.cpp)

