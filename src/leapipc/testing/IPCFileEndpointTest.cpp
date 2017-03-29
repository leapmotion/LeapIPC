// Copyright (C) 2012-2017 Leap Motion, Inc. All rights reserved.
#include "stdafx.h"
#include "IPCTestUtils.h"
#include <leapipc/IPCFileEndpoint.h>
#include <autowiring/autowiring.h>
#include <autowiring/CoreThread.h>
#include <cstdio>
#include <gtest/gtest.h>

using namespace leap::ipc;

class IPCFileEndpointTest :
  public testing::Test
{};

TEST_F(IPCFileEndpointTest, RawWriteToReadFromFile)
{
  std::string testMessage = "0123456789abcdef";
  const char* testFile = "IPCFileEndpoint_TestFile";

  // Write test message to file
  {
    std::shared_ptr<IPCFileEndpoint> ep = std::make_shared<IPCFileEndpoint>(testFile, false, true);
    ep->WriteRaw((void*)testMessage.c_str(), testMessage.size());
  }

  // Read test message from file
  {
    std::shared_ptr<IPCFileEndpoint> ep = std::make_shared<IPCFileEndpoint>(testFile, true, false);

    size_t bufsize = testMessage.size()+1;
    char* buf = new char[bufsize];
    buf[bufsize - 1] = 0;

    ep->ReadRaw((void*)buf, bufsize-1);

    ASSERT_EQ(strcmp(buf, testMessage.c_str()), 0);

    delete[] buf;
  }

  remove(testFile);
}

TEST_F(IPCFileEndpointTest, FileEndpointTwoChannels)
{
  std::string testMessage1 = "0123456789abcdef";
  std::string testMessage2 = "fedcba9876543210";
  const char* testFile = "IPCFileEndpoint_TestFile2";

  // Write test messages to file
  {
    std::shared_ptr<IPCFileEndpoint> ep = std::make_shared<IPCFileEndpoint>(testFile, false, true);
    auto channel0 = ep->AcquireChannel(0, IPCEndpoint::Channel::WRITE_ONLY);
    auto channel1 = ep->AcquireChannel(1, IPCEndpoint::Channel::WRITE_ONLY);

    channel0->Write(testMessage1.c_str(), testMessage1.size());
    channel0->WriteMessageComplete();
    channel1->Write(testMessage2.c_str(), testMessage2.size());
    channel1->WriteMessageComplete();
  }

  // Read test messages from file
  {
    std::shared_ptr<IPCFileEndpoint> ep = std::make_shared<IPCFileEndpoint>(testFile, true, false);

    size_t bufsize = testMessage1.size() + 1;
    char* buf = new char[bufsize];
    buf[bufsize - 1] = 0;

    // read first message
    IPCEndpoint::Header hdr = ep->ReadMessageHeader();
    ASSERT_EQ(hdr.channel, 0);
    ASSERT_EQ(hdr.PayloadSize(), testMessage1.size());
    ep->ReadPayload(buf, hdr.PayloadSize());
    ASSERT_EQ(strcmp(buf, testMessage1.c_str()), 0);
    delete[] buf;

    hdr = ep->ReadMessageHeader(); // read empty EOM header
    ASSERT_EQ(hdr.eom, 1);

    bufsize = testMessage2.size() + 1;
    buf = new char[bufsize];
    buf[bufsize - 1] = 0;

    // read second message
    hdr = ep->ReadMessageHeader();
    ASSERT_EQ(hdr.channel, 1);
    ASSERT_EQ(hdr.PayloadSize(), testMessage2.size());
    ep->ReadPayload(buf, hdr.PayloadSize());
    ASSERT_EQ(strcmp(buf, testMessage2.c_str()), 0);
    delete[] buf;

    hdr = ep->ReadMessageHeader(); // read empty EOM header
    ASSERT_EQ(hdr.eom, 1);
  }

  remove(testFile);
}
