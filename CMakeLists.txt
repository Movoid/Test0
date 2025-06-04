cmake_minimum_required(VERSION 3.25)
project(Test0 VERSION 0.0.1 LANGUAGES C CXX ASM)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/lib)

file(GLOB_RECURSE test0_CXX_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp)
file(GLOB_RECURSE test0_C_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/*.c)
file(GLOB_RECURSE test0_ASM_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/*.s)

# add_executable(test0 ${test0_CXX_SOURCES} ${test0_C_SOURCES} ${test0_ASM_SOURCES})
add_executable(tcp_inet_server ${CMAKE_CURRENT_SOURCE_DIR}/src/tcp_inet_server.cpp)
target_include_directories(tcp_inet_server PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)

# target_compile_options(test0 PRIVATE -fsanitize=address)
# target_link_options(test0 PRIVATE -fsanitize=address)
target_link_libraries(tcp_inet_server PRIVATE rpnparser)
target_link_libraries(tcp_inet_server PRIVATE mm_malloc)
target_link_libraries(tcp_inet_server PRIVATE wcnm)
target_link_libraries(tcp_inet_server PRIVATE crypt)
target_link_libraries(tcp_inet_server PRIVATE bsd)

# cmake_policy(SET CMP0167 NEW)
# find_package(Boost REQUIRED)
# target_link_libraries(test0 PRIVATE Boost::boost)

# final resolution 1
