#include "gtest/gtest.h"
#include <bits/stdc++.h>

TEST(GTestSamples, Sample1) {

  std::cout << "Hello GTest" << std::endl;

  ASSERT_EQ(1, 1);
}

TEST(GTestSamples, Sample2) {

  std::cout << "Hello GTest" << std::endl;

  ASSERT_EQ(1, 2) << "Sample2 wtf";
}