cmake_minimum_required(VERSION 3.12)

project(ProjCMK VERSION 0.1.2 DESCRIPTION "this is test desc." LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED true)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_subdirectory(./lib)

file(GLOB_RECURSE caonima_SOURCES ${CMAKE_SOURCE_DIR}/src/*.cpp)
add_executable(wocao ${caonima_SOURCES})

file(GLOB_RECURSE caonima_HEADERS 
  ${CMAKE_SOURCE_DIR}/include
  ${CMAKE_SOURCE_DIR}/lib
)

target_link_libraries(wocao PRIVATE test)
target_include_directories(wocao PRIVATE ${CMAKE_SOURCE_DIR}/lib)

configure_file(${CMAKE_SOURCE_DIR}/include/config.h.in ${CMAKE_BINARY_DIR}/include/config.h)
