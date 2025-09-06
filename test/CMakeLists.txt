enable_testing()

add_executable(
  hello_test
  samples/test_sample.cpp
)
target_link_libraries(
  hello_test
  GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(hello_test)

message("This is CMakeLists.txt in samples, my current path is " ${CMAKE_CURRENT_SOURCE_DIR})
message("This is CMakeLists.txt in samples, my pwd is " ${})