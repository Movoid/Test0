add_library(MyMath STATIC ./mymath.cpp)

target_include_directories(MyMath PUBLIC "${PROJECT_SOURCE_DIR}")
