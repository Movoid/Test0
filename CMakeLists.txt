cmake_minimum_required(VERSION 3.12)

project(ProjCMK VERSION 0.1.2 DESCRIPTION "this is test desc." LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED true)

configure_file(./include/config.h.in ./include/config.h)

add_executable(final_program ./src/main.cpp)

target_include_directories(final_program PUBLIC "${PROJECT_BINARY_DIR}/include"
    "${PROJECT_SOURCE_DIR}/lib"
    "${PROJECT_SOURCE_DIR}/lib/test")

add_subdirectory(./lib/test)

target_link_libraries(final_program PUBLIC test)
