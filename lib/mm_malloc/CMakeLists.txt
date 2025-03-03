cmake_minimum_required(VERSION 3.20)
project(Mm_malloc VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON) 
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_library(mm_malloc STATIC ${CMAKE_CURRENT_SOURCE_DIR}/mm_malloc_v1.cpp)
add_library(Mm_malloc::mm_malloc ALIAS mm_malloc)
target_include_directories(mm_malloc PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
