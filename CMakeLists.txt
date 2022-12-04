cmake_minimum_required(VERSION 3.24)
project(hllr)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE) # enables LTO

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread -flto")

add_executable(hllr main.cpp gc.cpp)
