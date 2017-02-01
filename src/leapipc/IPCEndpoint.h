// Copyright (C) 2012-2016 Leap Motion, Inc. All rights reserved.
#pragma once
#include "MessageBuffers.h"
#include <LeapSerial/Archive.h>
#include <mutex>
#include <memory>
#include <thread>
#include <atomic>
#include <autowiring/Autowired.h>
#include <cstdint>
#include <vector>

namespace leap {
namespace ipc {

/// <summary>
/// Platform-independent IPC client interface
/// </summary>
class IPCEndpoint :
  public std::enable_shared_from_this<IPCEndpoint>
{
public:
  IPCEndpoint(void);
  virtual ~IPCEndpoint(void);

  IPCEndpoint(const IPCEndpoint&) = delete;
  IPCEndpoint& operator=(const IPCEndpoint&) = delete;

  enum class Reason {
    // Unknown/cannot determine
    Unspecified = 1,

    // Connection was lost because the remote end was closed
    ConnectionLost,

    // User requested the connection be gracefully closed
    UserClosed,

    // User requested that the channel be aborted
    UserAborted,

    // Connection was abandoned due to stream integrity
    StreamIntegrityViolation,

    // An unrecoverable write failure occurred
    WriteFailure,

    // An unrecoverable read failure occurred
    ReadFailure,

    // A weak pointer could not be locked, system is tearing down
    WeakPointerLock,
  };

  /// <summary>
  /// Signal asserted when the server connection is lost
  /// </summary>
  /// <remarks>
  /// When this signal is asserted, the connection is already closed.  This signal
  /// will be asserted in response to the first call to Close or Abort, but only
  /// if the endpoint is not already closed.
  /// </remarks>
  autowiring::signal<void(Reason reason)> onConnectionLost;

  /// <summary>
  /// Represents a single channel in the endpoint
  /// </summary>
  /// <remarks>
  /// Methods on this type are not thread-safe with respect to each other.  Methods are thread safe with respect
  /// to other instances of the same type.  All static members are thread safe.
  /// </remarks>
  class Channel :
    public leap::IInputStream,
    public leap::IOutputStream
  {
  public:
    ~Channel();
    Channel(const Channel&) = delete;
    Channel& operator=(const Channel&) = delete;

    enum ChannelNumber {
      PROTOBUF = 0,
      FLATBUFFERS = 1
    };

    enum Mode { READ_ONLY, WRITE_ONLY, READ_WRITE };

    // Read a single, entire message consisting of possibly multiple partial buffers
    MessageBuffers::Buffers ReadMessageBuffers();

    // Write a single, entire message consisting of possibly multiple partial buffers
    bool WriteMessageBuffers(const MessageBuffers::Buffers& messageBuffers);

    /// <summary>
    /// Reads the requested number of bytes into the passed buffer
    /// </summary>
    /// <returns>
    /// The number of bytes that were actually read, or -1 if there is an error
    /// </returns>
    std::streamsize Read(void* buffer, std::streamsize size) override;

    /// <summary>
    /// Performs a write operation on this channel
    /// </summary>
    bool Write(const void* pBuf, std::streamsize nBytes) override;

    // Skip implementation, which works by making successive calls to Read
    std::streamsize Skip(std::streamsize count) override;

    // True if we have no more bytes to be read
    bool IsEof(void) const override { return m_endpoint->IsClosed(); }

    /// <summary>
    /// Called after an incoming message has been processed, allowing the stream to begin reading the next message
    /// </summary>
    void ReadMessageComplete();

    /// <summary>
    /// Called after a message has been fully sent, allowing the stream to begin writing the next message
    /// </summary>
    bool WriteMessageComplete();

  private:
    Channel(const std::shared_ptr<IPCEndpoint>& endpoint, uint32_t channel, Mode mode);

    const uint32_t m_channel;
    const Mode m_mode;
    std::shared_ptr<IPCEndpoint> m_endpoint;

    friend class IPCEndpoint;
  };

  struct Header {
    uint8_t magic1 = 0x64;
    uint8_t magic2 = 0x37;
    uint8_t eom : 1;
    uint8_t channel : 2;
    uint8_t reserved : 2;
    uint8_t version : 3;
    uint8_t size = 8;
    uint32_t payloadLength = 0;

    // Keep these CHANNELS constants in sync with the number of bits used to hold the channel
    static const uint32_t NUMBER_OF_CHANNELS = 1 << 2;
    static const uint32_t NUMBER_OF_CHANNELS_MASK = (NUMBER_OF_CHANNELS - 1);

    //
    //  0                   1                   2                   3
    //  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
    // +---------------+---------------+-----+-+-+-+-+-+---------------+
    // |               |               |     |R|R| C |E|     Header    |
    // |    Magic 1    |    Magic 2    | Ver |S|S| H |O|     Length    |
    // |      (8)      |      (8)      | (3) |1|2|(2)|M|      (8)      |
    // +---------------+---------------+-----+-+-+-+-+-+---------------+
    // |                        Payload Length                         |
    // |                             (32)                              |
    // +---------------------------------------------------------------+
    //
    //  Magic 1:               8 bits (0x64)
    //  Magic 2:               8 bits (0x37)
    //  Version:               3 bits (0)
    //  Reserved:              2 bits (0)
    //  Channel:               2 bits (0)
    //  End of Message (EOM):  1 bit  (1 = Final fragment, 0 = More fragments to follow)
    //  Header Length:         8 bits (8)
    //  Payload Length:       32 bits (Length of payload to follow)
    //
    Header(void) :
      eom(false),
      channel(0),
      reserved(0),
      version(0)
    {}

    uint32_t Version() const { return version; }
    void SetVersion(uint32_t version) { this->version = (uint8_t)version; }
    uint32_t Channel() const { return channel; }
    void SetChannel(uint32_t channel) { this->channel = channel; }
    bool IsChannel(uint32_t channel) const { return this->channel == channel; }
    bool IsEndOfMessage() const { return eom; }
    void SetEndOfMessage(bool eom = true) { this->eom = eom; }
    void ClearEndOfMessage() { eom = false; }
    uint32_t Size() const { return size; }
    uint32_t PayloadSize() const {
      uint8_t* p = (uint8_t*)&payloadLength;
      return (p[0] << 24) + (p[1] << 16) + (p[2] << 8) + p[3];
    }
    void SetPayloadSize(uint32_t size) {
      uint8_t* p = (uint8_t*)&payloadLength;
      p[0] = (size >> 24) & 0xFF;
      p[1] = (size >> 16) & 0xFF;
      p[2] = (size >> 8 ) & 0xFF;
      p[3] =  size & 0xFF;
    }
    bool Validate() const {
      return magic1 == 0x64 && magic2 == 0x37 && size >= sizeof(*this);
    }
  };

protected:
  // Low-level raw read/write functions (platform specific)
  // This is a blocking call
  virtual std::streamsize ReadRaw(void* buffer, std::streamsize size) = 0;
  virtual bool WriteRaw(const void* pBuf, std::streamsize nBytes) = 0;

  // Helper routine to receive exactly the specified number of bytes, or fail
  bool ReadRawN(void* buf, std::streamsize size);

  // Mark endpoint as closed, and notify others that may not yet know
  void Close(Reason reason);

  // PID of the remote endpoint
  uint32_t m_pid = 0;

private:
  void ReleaseChannel(uint32_t channel, Channel::Mode mode);
  void HandlePendingUnsafe();

  // Low-level read; either into a pre-allocated buffer, or create a buffer big enough to hold the (partial) message
  std::streamsize Read(uint32_t channel, void* buffer, std::streamsize size, MessageBuffers::SharedBuffer* sharedBuffer);

  MessageBuffers::Buffers ReadMessageBuffers(uint32_t channel);
  bool WriteMessageBuffers(uint32_t channel, const MessageBuffers::Buffers& messageBuffers);

  std::streamsize Read(uint32_t channel, void* buffer, std::streamsize size);

  /// <summary>
  /// Performs a write operation on this channel and optionally marks this payload as the last one in the current message
  /// </summary>
  /// <param name="isComplete">Set if this is the last payload in the message</param>
  /// <remarks>
  /// The WriteMessageComplete function causes the transmission of a zero-width message payload.  This routine
  /// prevents this from being necessary.  If this routine is called with a zero-width buffer and end sent to true,
  /// it is identical in behavior to WriteMessageComplete.
  /// </remarks>
  bool Write(uint32_t channel, const void* pBuf, std::streamsize nBytes, bool isComplete);

  std::streamsize Skip(uint32_t channel, std::streamsize count);
  void ReadMessageComplete(uint32_t channel);
  bool WriteMessageComplete(uint32_t channel);

  struct Message {
    void BeginHeader() { *this = {}; }
    void BeginPayload() {
      length = header.PayloadSize();
      position = 0;
      isProcessingHeader = false;
    }

    // Header itself
    Header header;

    // Length of header or payload
    uint32_t length = sizeof(Header);

    // Position in the header or payload
    uint32_t position = 0;

    // Flag of whether we are processing the header or the payload
    bool isProcessingHeader = true;
  };

  enum { DRAIN_SIZE = 16384 };

  struct Handlers {
    bool pending;
    bool reading;
    bool writing;
    std::atomic<bool> eom;
  };

  Autowired<MessageBuffers::SharedBufferPool> m_sharedBufferPool;
  std::vector<uint8_t> m_drain; // Buffer in which to dump unwanted data
  std::mutex m_sendMutex;
  std::mutex m_recvMutex;
  std::mutex m_pendingMutex;
  std::condition_variable m_recvCondition;
  Header m_sendHeader;
  Message m_recvMessage;
  const std::streamsize m_blockSize;
  Handlers m_handler[Header::NUMBER_OF_CHANNELS];
  std::atomic<bool> m_hasPending{ false };
  std::atomic<bool> m_isClosed{ false };

  // Last header read by ReadMessageHeader
  Header m_lastHeader;

  // Number of bytes remaining in the current message
  size_t m_nRemain = 0;

public:
  bool IsClosed() const volatile { return m_isClosed; }
  const Header& GetLastHeader(void) const { return m_lastHeader; }
  size_t GetPayloadRemaining(void) const { return m_nRemain; }

  /// <summary>
  /// Acquires a channel that can be used to transmit messages
  /// </summary>
  std::unique_ptr<Channel> AcquireChannel(uint32_t channel, Channel::Mode mode);

  /// <summary>
  /// Reads the next message header in the stream
  /// </summary>
  /// <returns>
  /// The returned header is used to determine how many bytes to read in a subsequent call to
  /// ReadPayload.  It is an error to call this routine if any bytes remain to be read from the
  /// last message.
  /// </returns>
  const Header& ReadMessageHeader(void);

  /// <summary>
  /// Reads the payload from the most recently received message header
  /// </summary>
  /// <returns>The the number of bytes actually read</returns>
  /// <remarks>
  /// This method returns zero if there are no more bytes in the current message to be read.  This
  /// method will not read past the end of the current payload's end.
  /// </remarks>
  std::streamsize ReadPayload(void* pBuf, size_t ncb);

  /// <summary>
  /// Returns the PID of the remote endpoint
  /// </summary>
  uint32_t PeerProcessId() const { return m_pid; }

  /// <summary>
  /// Abandons any blocking operations and prepares this connection for termination
  /// </summary>
  /// <param name="reason">The reason the connection is being aborted</param>
  /// <remarks>
  /// Abort only attempts to abandon operations that are scheduled; operations that are currently
  /// underway may or may not be completed after this call.  There may potentially be operations
  /// underway after this call returns.  Attempts may be made to schedule operations after Abort
  /// is called; these attempts will throw exceptions.
  /// </remarks>
  /// <returns>The first call will return true, subsequent calls will return false</returns>
  virtual bool Abort(Reason reason = Reason::UserAborted) = 0;
};

}}
