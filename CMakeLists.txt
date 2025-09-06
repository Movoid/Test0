cmake_minimum_required(VERSION 3.25)
project(Test0 VERSION 0.0.1 LANGUAGES C CXX ASM)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# git submodule 比 FetchContent 好用.

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/lib)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/test)

file(GLOB_RECURSE test0_CXX_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp)
file(GLOB_RECURSE test0_C_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/*.c)
file(GLOB_RECURSE test0_ASM_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/*.s)

# add_executable(test3 ${test0_CXX_SOURCES} ${test0_C_SOURCES} ${test0_ASM_SOURCES})
# target_include_directories(test3 PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)

# target_compile_options(test0 PRIVATE -fsanitize=address)
# target_link_options(test0 PRIVATE -fsanitize=address)
# target_link_libraries(test3 PRIVATE rpnparser)
# target_link_libraries(test3 PRIVATE mm_malloc)
# target_link_libraries(test3 PRIVATE wcnm)

# target_link_libraries(test3 PRIVATE crypt)
# target_link_libraries(test3 PRIVATE bsd)

# cmake_policy(SET CMP0167 NEW)
# find_package(Boost REQUIRED)
# target_link_libraries(test3 PRIVATE Boost::boost)
add_executable(qsbr src/qsbr.cpp)
target_include_directories(qsbr PUBLIC src/)

add_executable(qsbr_test src/qsbr_impl_stack.cpp)
target_include_directories(qsbr PUBLIC src/)


include(GNUInstallDirs)

# install(TARGETS test3
#   EXPORT ${PROJECT_NAME}Targets
#   RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
#   LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
#   ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
# )
