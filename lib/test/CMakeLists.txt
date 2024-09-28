add_library(test STATIC ./mymath.cpp)

target_include_directories(test PUBLIC "${PROJECT_SOURCE_DIR}")
