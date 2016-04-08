// Copyright (C) 2012-2016 Leap Motion, Inc. All rights reserved.
#define NOMINMAX
#include <gtest/gtest-all.cc>
#include <autowiring/AutowiringEnclosure.h>

using namespace std;

int main(int argc, const char* argv[])
{
  auto& listeners = testing::UnitTest::GetInstance()->listeners();
  listeners.Append(new AutowiringEnclosure);
  testing::InitGoogleTest(&argc, (char**)argv);
  return RUN_ALL_TESTS();
}
