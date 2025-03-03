cmake_minimum_required(VERSION 3.20)
project(RPNExprParser VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON) 
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_library(rpnparser INTERFACE)
add_library(RPNExprParser::rpnexprparser ALIAS rpnparser)
target_include_directories(rpnparser INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
