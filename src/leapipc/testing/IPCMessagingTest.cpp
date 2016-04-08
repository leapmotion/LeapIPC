// Copyright (C) 2012-2016 Leap Motion, Inc. All rights reserved.
#include "stdafx.h"
#include "IPCTestUtils.h"
#include <autowiring/autowiring.h>
#include <autowiring/CoreThread.h>
#include <leapipc/IPCClient.h>
#include <leapipc/IPCEndpoint.h>
#include <leapipc/IPCListener.h>
#include <gtest/gtest.h>
#include <thread>
#include FUTURE_HEADER

using namespace leap::ipc;

class IPCMessagingTest:
  public testing::Test
{
public:
  IPCMessagingTest(void) {
    AutoCurrentContext()->Initiate();
  }
};

struct Message {
  uint32_t id1;
  uint32_t id2;
  uint32_t id3;
  uint32_t id4;
};

TEST_F(IPCMessagingTest, SequentialMessageTransmission)
{
  AutoCurrentContext ctxt;
  std::string ns = GenerateNamespaceName();

  // Create client and server
  AutoConstruct<IPCClient> client(IPCTestScope(), ns.c_str());
  AutoConstruct<IPCListener> listener(IPCTestScope(), ns.c_str());

  auto val = std::make_shared<std::promise<int>>();
  listener->onClientConnected += [&val](const std::shared_ptr<IPCEndpoint>& ep) {
    AutoCreateContext ctxt;
    ctxt->Add(ep);

    auto channel = ep->AcquireChannel(0, IPCEndpoint::Channel::READ_ONLY);
    int nMessages = 0;
    while (!ep->IsClosed()) {
      auto buffers = channel->ReadMessageBuffers();
      if (buffers.size() != 4)
        break;
      nMessages++;
    }
    val->set_value(nMessages);
  };

  auto ep = client->Connect(std::chrono::minutes(1));
  ASSERT_NE(nullptr, ep) << " Failed to connect in time";

  {
    auto channel = ep->AcquireChannel(0, IPCEndpoint::Channel::WRITE_ONLY);
    for (uint32_t i = 0; i < 300; i++) {
      Message message{ i, i + 1, i + 2, i + 3 };
      channel->Write(&message, sizeof(message));
      channel->Write(&message, sizeof(message));
      channel->Write(&message, sizeof(message));
      channel->Write(&message, sizeof(message));

      // Indicate that the entire message has been written
      channel->WriteMessageComplete();
    }
  }

  // Close connection, shut down context:
  ep.reset();
  ctxt->SignalShutdown();

  // Block until gatherer stops:
  auto f = val->get_future();
  ASSERT_EQ(std::future_status::ready, f.wait_for(std::chrono::minutes(1))) << "Server did not receive messages in a timely fashion";
  ASSERT_EQ(300, f.get()) << "Not all messages were received as expected";
}

