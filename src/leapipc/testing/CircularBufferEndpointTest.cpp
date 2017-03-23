// Copyright (C) 2012-2017 Leap Motion, Inc. All rights reserved.
#include "stdafx.h"
#include "IPCTestUtils.h"
#include <leapipc/CircularBufferEndpoint.h>
#include <autowiring/autowiring.h>
#include <autowiring/CoreThread.h>
#include <cstdio>
#include <thread>
#include <gtest/gtest.h>

using namespace leap::ipc;

class CircularBufferEndpointTest :
  public testing::Test
{};

TEST_F(CircularBufferEndpointTest, WriteReadSequence)
{
  auto cbuf = std::make_shared<CircularBufferEndpoint>(32);

  // read from buffer in separate thread
  std::thread reader([cbuf] {
    char t[16];
    cbuf->ReadRaw(t, 4);
    ASSERT_EQ(strncmp(t, "abcd", 4), 0);
    cbuf->ReadRaw(t, 8);
    ASSERT_EQ(strncmp(t, "efghijkl", 8), 0);
    cbuf->ReadRaw(t, 16);
    ASSERT_EQ(strncmp(t, "mnopqr0123456789", 16), 0);
    cbuf->ReadRaw(t, 6);
    ASSERT_EQ(strncmp(t, "987654", 6), 0);
    cbuf->ReadRaw(t, 4);
    ASSERT_EQ(strncmp(t, "3210", 4), 0);

    cbuf->ReadRaw(t, 4);
    ASSERT_EQ(strncmp(t, "abcd", 4), 0);
    cbuf->ReadRaw(t, 8);
    ASSERT_EQ(strncmp(t, "efghijkl", 8), 0);
    cbuf->ReadRaw(t, 16);
    ASSERT_EQ(strncmp(t, "mnopqr0123456789", 16), 0);
    cbuf->ReadRaw(t, 6);
    ASSERT_EQ(strncmp(t, "987654", 6), 0);
    cbuf->ReadRaw(t, 4);
    ASSERT_EQ(strncmp(t, "3210", 4), 0);
  });

  // write to buffer
  cbuf->WriteRaw("abcdef", 6);
  cbuf->WriteRaw("ghijkl", 6);
  cbuf->WriteRaw("mnopqr", 6);
  cbuf->WriteRaw("01234567899876543210", 20);

  cbuf->WriteRaw("abcdef", 6);
  cbuf->WriteRaw("ghijkl", 6);
  cbuf->WriteRaw("mnopqr", 6);
  cbuf->WriteRaw("01234567899876543210", 20);

  reader.join();
}

TEST_F(CircularBufferEndpointTest, IncreaseSize)
{
  auto cbuf = std::make_shared<CircularBufferEndpoint>(16);

  // read from buffer in separate thread
  std::thread reader([cbuf] {
    char t[16];
    cbuf->ReadRaw(t, 4);
    ASSERT_EQ(strncmp(t, "abcd", 4), 0);
    cbuf->ReadRaw(t, 8);
    ASSERT_EQ(strncmp(t, "efghijkl", 8), 0);
    cbuf->ReadRaw(t, 16);
    ASSERT_EQ(strncmp(t, "mnopqr0123456789", 16), 0);
    cbuf->ReadRaw(t, 6);
    ASSERT_EQ(strncmp(t, "987654", 6), 0);
    cbuf->ReadRaw(t, 4);
    ASSERT_EQ(strncmp(t, "3210", 4), 0);

    cbuf->ReadRaw(t, 4);
    ASSERT_EQ(strncmp(t, "abcd", 4), 0);
    cbuf->ReadRaw(t, 8);
    ASSERT_EQ(strncmp(t, "efghijkl", 8), 0);
    cbuf->ReadRaw(t, 16);
    ASSERT_EQ(strncmp(t, "mnopqr0123456789", 16), 0);
    cbuf->ReadRaw(t, 6);
    ASSERT_EQ(strncmp(t, "987654", 6), 0);
    cbuf->ReadRaw(t, 4);
    ASSERT_EQ(strncmp(t, "3210", 4), 0);
  });

  // write to buffer
  cbuf->WriteRaw("abcdef", 6);
  cbuf->WriteRaw("ghijkl", 6);
  cbuf->WriteRaw("mnopqr", 6);
  cbuf->WriteRaw("01234567899876543210", 20);

  cbuf->WriteRaw("abcdef", 6);
  cbuf->WriteRaw("ghijkl", 6);
  cbuf->WriteRaw("mnopqr", 6);
  cbuf->WriteRaw("01234567899876543210", 20);

  reader.join();
}
