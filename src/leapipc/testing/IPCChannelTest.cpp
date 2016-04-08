#include "stdafx.h"
#include "IPCTestUtils.h"
#include <leapipc/IPCClient.h>
#include <leapipc/IPCEndpoint.h>
#include <leapipc/IPCListener.h>
#include <autowiring/autowiring.h>
#include <autowiring/CoreThread.h>
#include <gtest/gtest.h>

using namespace leap::ipc;

class IPCChannelTest:
  public testing::Test
{
public:
  std::string m_namespaceName = GenerateNamespaceName();;
};

TEST_F(IPCChannelTest, MultipleChannelInstances)
{
  AutoCurrentContext ctxt;
  ctxt->Initiate();

  // Create client and server
  AutoConstruct<IPCListener> server(IPCTestScope(), m_namespaceName.c_str());
  AutoConstruct<IPCClient> client(IPCTestScope(), m_namespaceName.c_str());

  auto ep = client->Connect(std::chrono::seconds(15));
  ASSERT_NE(nullptr, ep) << "Client failed to connect to the server";

  // Attempt to open a channel for read/write twice -- should only succeed the first time
  {
    auto channel1 = ep->AcquireChannel(0, IPCEndpoint::Channel::READ_WRITE);
    ASSERT_NE(nullptr, channel1) << "Failed to obtain first channel";
    auto channel2 = ep->AcquireChannel(0, IPCEndpoint::Channel::READ_WRITE);
    ASSERT_EQ(nullptr, channel2) << "Obtained a channel that should have already been checked out";
  }

  // Attempt to open a channel twice, once for read and once for write -- should succeed for both
  {
    auto channel1 = ep->AcquireChannel(1, IPCEndpoint::Channel::READ_ONLY);
    ASSERT_NE(nullptr, channel1) << "Failed to obtain a read-only channel";
    auto channel2 = ep->AcquireChannel(1, IPCEndpoint::Channel::WRITE_ONLY);
    ASSERT_NE(nullptr, channel2) << "Failed to obtain the write portion of a channel only open for reading";
  }

  // Attempt to open a channel twice, once for read and once for read/write -- should only succeed the first time
  {
    auto channel1 = ep->AcquireChannel(2, IPCEndpoint::Channel::READ_ONLY);
    ASSERT_NE(nullptr, channel1) << "Failed to obtain a read-only channel";
    auto channel2 = ep->AcquireChannel(2, IPCEndpoint::Channel::READ_WRITE);
    ASSERT_EQ(nullptr, channel2) << "Incorrectly obtained a channel for read/write after already obtaining it for read";
  }

  // Attempt to open a channel for read/write twice, but after releasing the first one -- should succeed for both
  {
    auto channel = ep->AcquireChannel(3, IPCEndpoint::Channel::READ_WRITE);
    ASSERT_NE(nullptr, channel) << "Failed to obtain a read/write channel";
    channel.reset();
    channel = ep->AcquireChannel(3, IPCEndpoint::Channel::READ_WRITE);
    ASSERT_NE(nullptr, channel) << "Failed to reobtain a channel for read/write after releasing it";
  }
}
