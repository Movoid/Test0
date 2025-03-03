cmake_minimum_required(VERSION 3.20)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

file(GLOB test_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)
add_library(test STATIC ${test_SOURCES})
target_include_directories(test PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
