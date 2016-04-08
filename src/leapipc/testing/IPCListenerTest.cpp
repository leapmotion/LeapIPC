#include "stdafx.h"
#include "IPCClient.h"
#include "IPCEndpoint.h"
#include "IPCListener.h"
#include "IPCTestUtils.h"
#include <autowiring/autowiring.h>
#include <autowiring/Bolt.h>
#include <autowiring/ContextEnumerator.h>
#include <autowiring/CoreThread.h>
#include <autowiring/ExceptionFilter.h>
#include <thread>
#include <gtest/gtest.h>
#include <system_error>
#include FUTURE_HEADER

static const int sc_nTestMessages = 20;

class IPCListenerTest:
  public testing::Test
{
public:
  IPCListenerTest(void) {
    AutoCurrentContext()->Initiate();
  }

  // A namespace generated randomly on a per-test basis
  const std::string m_namespaceName = GenerateNamespaceName();
};

TEST_F(IPCListenerTest, ClientTerminationWithNoServer) {
  AutoCurrentContext ctxt;

  // Create a client listener which we will, in this case, never be satisfied:
  AutoConstruct<IPCClient> client{ IPCTestScope(), m_namespaceName.c_str() };
  ASSERT_EQ(nullptr, client->Connect(std::chrono::milliseconds(250))) << "Client connected unexpectedly";
}

TEST_F(IPCListenerTest, VerifySimpleServerCreation) {
  AutoCurrentContext ctxt;

  // Listener should spam the client with messages and then exit:
  AutoConstruct<IPCListener> listener(IPCTestScope(), m_namespaceName.c_str());
  auto termInTime = std::make_shared<bool>(true);
  listener->onClientConnected += [termInTime](const std::shared_ptr<IPCEndpoint>& ep) {
    AutoCreateContext ctxt;
    ctxt->Initiate();
    ctxt->Add(ep);
    auto channel = ep->AcquireChannel(0, IPCEndpoint::Channel::Mode::WRITE_ONLY);
    for (int i = 0; i < sc_nTestMessages; i++) {
      channel->Write(&i, sizeof(i));
      channel->WriteMessageComplete();
    }

    // Must terminate the context before we return control otherwise it will just hang around
    ctxt->SignalShutdown();
    *termInTime = ctxt->Wait(std::chrono::seconds(5));
  };

  // Now create a client and start that context, too:
  AutoConstruct<IPCClient> client(IPCTestScope(), m_namespaceName.c_str());

  // Try to connect and obtain a channel:
  std::shared_ptr<IPCEndpoint> endpoint = client->Connect(std::chrono::seconds(1));
  ASSERT_NE(nullptr, endpoint) << "Client took too long to connect to the server";
  auto channel = endpoint->AcquireChannel(0, IPCEndpoint::Channel::READ_ONLY);

  std::vector<unsigned char> output;
  for (;;) {
    unsigned char buf[256];
    std::streamsize nRead = channel->Read(buf, sizeof(buf));
    if (nRead < 0)
      // Termination criteria:
      break;
    else if (nRead == 0)
      channel->ReadMessageComplete();

    output.insert(output.end(), buf, buf + nRead);
  }

  ctxt->SignalShutdown();
  ASSERT_TRUE(*termInTime) << "Writer context did not exit in a timely fashion";
  ASSERT_TRUE(client->WaitFor(std::chrono::seconds(5))) << "Client connector failed to shut down in a timely fashion";
  ASSERT_TRUE(listener->WaitFor(std::chrono::seconds(5))) << "Listener failed to shut down in a timely fashion";
}

TEST_F(IPCListenerTest, ServerTerminationWithNoClient) {
  // Create a client listener which we will, in this case, never be satisfied:
  AutoConstruct<IPCListener> listener{ IPCTestScope(), m_namespaceName.c_str() };
  AutoCurrentContext()->Initiate();

  bool hit = false;
  listener->onClientConnected += [&](const std::shared_ptr<IPCEndpoint>&) { hit = true; };

  // Let the client run for a brief period of time.  It should never create a client context
  std::this_thread::sleep_for(std::chrono::milliseconds(250));
  ASSERT_FALSE(hit) << "A connection was unexpectedly received";

  // Now verify that we can gracefully terminate this listener
  listener->Stop();
  ASSERT_TRUE(listener->WaitFor(std::chrono::seconds(5))) << "Listener did not shut down in a timely fashion";
}

TEST_F(IPCListenerTest, ServerCleansItselfUp) {
  AutoCurrentContext ctxt;

  // Create a server in a subcontext, kick it off for a bit, and then shut it down
  {
    AutoCreateContext serverContext;
    serverContext->Inject<IPCListener>(IPCTestScope(), m_namespaceName.c_str());
    serverContext->Initiate();
    ASSERT_FALSE(serverContext->Wait(std::chrono::milliseconds(10))) << "Server context unexpectedly terminated";
    serverContext->SignalShutdown();
    ASSERT_TRUE(serverContext->Wait(std::chrono::minutes(1))) << "Server context did not terminate in a timely fashion";
  }

  // Verify we can terminate a connection request underway, and that it doesn't succeed after the server should have been terminated
  auto client = ctxt->Inject<IPCClient>(IPCTestScope(), m_namespaceName.c_str());
  auto f = std::async(
    std::launch::async,
    [client] { client->Connect(); }
  );

  // Verify that the client doesn't terminate prematurely, but then the blocking call backs out when we ask it to do so
  ASSERT_EQ(std::future_status::timeout, f.wait_for(std::chrono::milliseconds(250))) << "Client unexpectedly connected";
  client->Stop();
  ASSERT_EQ(
    std::future_status::ready,
    f.wait_for(std::chrono::minutes(1))
  ) << "Client context did not terminate in a timely fashion";
}

class TerminatesEnclosingScopeOnException:
  public ContextMember,
  public ExceptionFilter
{
public:
  TerminatesEnclosingScopeOnException(void):
    exceptionOccurred(false)
  {}

  bool exceptionOccurred;

  void Filter(void) override {
    try {
      throw;
    }
    catch(std::system_error& err) {
      if(err.code().category() == std::generic_category()) {
        switch(std::errc(err.code().value())) {
        case std::errc::not_connected:
        case std::errc::broken_pipe:
          // No problem, we can tolerate these conditions
          return;
        default:
          // Fall through
          break;
        }
      }
    }

    exceptionOccurred = true;
    GetContext()->SignalShutdown();
  }
};

TEST_F(IPCListenerTest, BandwidthSaturationTest) {
  AutoCurrentContext ctxt;
  AutoRequired<TerminatesEnclosingScopeOnException> exceptionChecker;

  // Create a client and server.  Server will saturate the downstream link to the client.
  AutoConstruct<IPCListener> pListener{ IPCTestScope(), m_namespaceName.c_str() };

  // Want to extract when a connection occurs:
  auto ppServerEp = std::make_shared<std::shared_ptr<IPCEndpoint>>();
  pListener->onClientConnected += [ppServerEp] (const std::shared_ptr<IPCEndpoint>& endpoint) {
    *ppServerEp = endpoint;
  };

  // Set up the client connection:
  AutoConstruct<IPCClient> client{ IPCTestScope(),m_namespaceName.c_str() };
  std::shared_ptr<IPCEndpoint> clientEp = client->Connect(std::chrono::seconds(3));
  ASSERT_NE(nullptr, clientEp) << "Client took too long to connect";

  // Delay for the endpoint to be satisfied.  God I hate sleep...but Android doesn't support promises
  for (size_t i = 0; !*ppServerEp && i < 100; i++)
    std::this_thread::sleep_for(std::chrono::milliseconds(25));

  // Set up a server channel, dispatch from it:
  ASSERT_NE(nullptr, *ppServerEp) << "Server took too long to connect";

  // Read operation has to be asynchronous, or the pipe will block:
  auto nBytesTotal = std::make_shared<std::streamsize>(0L);
  auto r = std::async(
    std::launch::async,
    [ppServerEp, nBytesTotal] {
      auto& serverEp = **ppServerEp;
      auto serverChannel = serverEp.AcquireChannel(0, IPCEndpoint::Channel::Mode::READ_ONLY);
      if (!serverChannel)
        return;

      std::vector<uint8_t> msg(4 * 1024 * 1024);
      while(!serverEp.IsClosed()) {
        std::streamsize nBytes = serverChannel->Read(msg.data(), (int)msg.size());
        if (0 < nBytes)
          *nBytesTotal += nBytes;
      }
    }
  );

  std::chrono::nanoseconds dt{ 0 };
  {
    auto clientChannel = clientEp->AcquireChannel(0, IPCEndpoint::Channel::Mode::WRITE_ONLY);
    std::vector<uint8_t> msg(128 * 1024);

    // We want to send 500 messages with 4MB each
    auto t = std::chrono::profiling_clock::now();
    for (size_t i = 0; i < 500; i++)
      clientChannel->Write(msg.data(), (int)msg.size());
    dt = std::chrono::profiling_clock::now() - t;
  }

  // Shut down our end of the link:
  clientEp->Abort();

  // Server should complete shortly afterwards:
  ASSERT_EQ(std::future_status::ready, r.wait_for(std::chrono::minutes(10))) << "Took too long to transfer 64MB of data";

  // Make sure that nothing went wrong
  ASSERT_FALSE(exceptionChecker->exceptionOccurred) << "An unexpected exception occurred while transmitting messages to the server";

  // Terminate everything, validate expected receipt behavior:
  ctxt->SignalShutdown(false);
  ASSERT_TRUE(ctxt->Wait(std::chrono::seconds(10)));
}
