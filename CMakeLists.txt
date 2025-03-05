cmake_minimum_required(VERSION 3.20)
project(Test0 VERSION 0.0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON) 
set(CMAKE_EXPORT_COMPILE_COMMANDS ON) 

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/lib)

file(GLOB_RECURSE test0_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp)
add_executable(test0 ${test0_SOURCES})

target_link_libraries(test0 PRIVATE rpnparser)
target_link_libraries(test0 PRIVATE mm_malloc)
target_link_libraries(test0 PRIVATE wcnm)

# cmake_policy(SET CMP0167 NEW)
# find_package(Boost REQUIRED)
# target_link_libraries(test0 PRIVATE Boost::boost)


# this is main2