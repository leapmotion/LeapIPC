#include "stdafx.h"
#include "IPCTestUtils.h"
#include <autowiring/autowiring.h>
#include <leapipc/IPCClient.h>
#include <leapipc/IPCEndpoint.h>
#include <leapipc/IPCListener.h>
#include <gtest/gtest.h>

using namespace leap::ipc;

class IPCShutdownTest:
  public testing::Test
{
public:
  IPCShutdownTest(void):
    m_namespaceName(GenerateNamespaceName())
  {
    AutoCurrentContext()->Initiate();
  }

  std::string m_namespaceName;
};

TEST_F(IPCShutdownTest, ValidateAbortCorrectlyTerminates) {
  AutoCurrentContext ctxt;
  ctxt->Initiate();

  // Setup:
  AutoConstruct<IPCListener> mylistener{ IPCTestScope(), m_namespaceName.c_str() };
  AutoConstruct<IPCClient> myclient{ IPCTestScope(), m_namespaceName.c_str() };

  // Server should write a few messages to the client:
  mylistener->onClientConnected += [&mylistener] (const std::shared_ptr<IPCEndpoint>& ep) {
    AutoCreateContext ctxt;
    ctxt->Add(ep);
    ctxt->Initiate();

    char buf[] = "Hello world!";
    auto channel = ep->AcquireChannel(3, IPCEndpoint::Channel::READ_WRITE);
    channel->Write(buf, sizeof(buf));
    channel->WriteMessageComplete();
    channel->Read(buf, 2);
    ctxt->SignalShutdown();
  };

  // Download data from the server
  {
    auto ep = myclient->Connect(std::chrono::minutes(1));
    auto channel = ep->AcquireChannel(3, IPCEndpoint::Channel::READ_ONLY);
    channel->Write("hi", 2);

    char buf[32];
    while (!ep->IsClosed()) {
      channel->Read(buf, sizeof(buf));
      channel->ReadMessageComplete();
    }
  }

  // Shut everything down, this should be enough to trigger a correct abort behavior
  ctxt->SignalShutdown();
  ASSERT_TRUE(ctxt->Wait(std::chrono::minutes(1))) << "Context took too long to tear down";
}
