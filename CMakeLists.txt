cmake_minimum_required(VERSION 3.14)
project(zrtools CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(ZRT_BUILD_TESTS "Build zrtools unit tests" ON)

# src/ 作为公共 include 根，支持 #include "zrtools/xxx.h"
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src)

add_subdirectory(src/zrtools)

if (ZRT_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
