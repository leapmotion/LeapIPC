// Copyright (C) 2012-2018 Leap Motion, Inc. All rights reserved.
#include "stdafx.h"
#include "IPCTestUtils.h"
#include <leapipc/IPCClient.h>
#include <leapipc/IPCEndpoint.h>
#include <leapipc/IPCListener.h>
#include <gtest/gtest.h>

using namespace leap::ipc;

class IPCFormatTest :
  public testing::Test
{};

static_assert(sizeof(IPCEndpoint::Header) == 8, "Header size mismatch");

TEST_F(IPCFormatTest, HeaderFormat) {
  // Known-good data, we need to be able to parse this one:
  static const uint8_t data[8] = {0x64, 0x37, 0x83, 8, 0xDE, 0xAD, 0xBE, 0xEF};
  const IPCEndpoint::Header& hdr = *(IPCEndpoint::Header*)data;

  ASSERT_TRUE(hdr.Validate());
  ASSERT_EQ(4u, hdr.Version());
  ASSERT_EQ(1u, hdr.Channel());
  ASSERT_TRUE(hdr.IsEndOfMessage());
  ASSERT_EQ(8u, hdr.Size());
  ASSERT_EQ(0xDEADBEEF, hdr.PayloadSize());

  IPCEndpoint::Header refHdr;
  refHdr.SetVersion(4);
  refHdr.SetChannel(1);
  refHdr.SetEndOfMessage();
  refHdr.SetPayloadSize(0xDEADBEEF);
  ASSERT_EQ(0, memcmp(&hdr, &refHdr, sizeof(hdr))) << "Header mutator methods must result in a binary-equivalent header to the reference";
}
