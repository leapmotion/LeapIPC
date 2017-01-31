// Copyright (C) 2012-2016 Leap Motion, Inc. All rights reserved.
#include "stdafx.h"
#include "IPCEndpoint.h"
#include <stdexcept>

using namespace leap::ipc;

//
// IPCEndpoint::Channel
//

IPCEndpoint::Channel::Channel(const std::shared_ptr<IPCEndpoint>& endpoint, uint32_t channel, Mode mode) :
  m_endpoint{endpoint},
  m_channel{channel},
  m_mode{mode}
{
}

IPCEndpoint::Channel::~Channel()
{
  if (m_endpoint)
    m_endpoint->ReleaseChannel(m_channel, m_mode);
}

MessageBuffers::Buffers IPCEndpoint::Channel::ReadMessageBuffers() {
  return m_endpoint->ReadMessageBuffers(m_channel);
}

bool IPCEndpoint::Channel::WriteMessageBuffers(const MessageBuffers::Buffers& messageBuffers) {
  return m_endpoint->WriteMessageBuffers(m_channel, messageBuffers);
}

std::streamsize IPCEndpoint::Channel::Read(void* buffer, std::streamsize size) {
  return m_endpoint->Read(m_channel, buffer, size);
}

bool IPCEndpoint::Channel::Write(const void* pBuf, std::streamsize nBytes) {
  return m_endpoint->Write(m_channel, pBuf, nBytes, false);
}

std::streamsize IPCEndpoint::Channel::Skip(std::streamsize count) {
  return m_endpoint->Skip(m_channel, count);
}

void IPCEndpoint::Channel::ReadMessageComplete() {
  m_endpoint->ReadMessageComplete(m_channel);
}

bool IPCEndpoint::Channel::WriteMessageComplete() {
  return m_endpoint->WriteMessageComplete(m_channel);
}

//
// IPCEndpoint
//

IPCEndpoint::IPCEndpoint(void) :
  m_blockSize{ 0x7FFFFFFF },
  m_drain(DRAIN_SIZE, 0)
{
  for (uint32_t channel = 0; channel < Header::NUMBER_OF_CHANNELS; channel++) {
    auto& handler = m_handler[channel];
    handler.pending = false;
    handler.reading = false;
    handler.writing = false;
    handler.eom = true;
  }
}

IPCEndpoint::~IPCEndpoint(void) {}

std::unique_ptr<IPCEndpoint::Channel> IPCEndpoint::AcquireChannel(uint32_t channel, Channel::Mode mode) {
  if (channel >= Header::NUMBER_OF_CHANNELS)
    return nullptr;

  // Verify we aren't already checked out:
  std::lock_guard<std::mutex> lock(m_pendingMutex);

  // Decide what to do based on the mode, and do a permissions check so we don't check out a handler twice
  auto& handler = m_handler[channel];
  switch (mode) {
  case Channel::READ_ONLY:
    if (handler.pending || handler.reading)
      return nullptr;

    m_hasPending = true;
    handler.pending = true;
    break;
  case Channel::WRITE_ONLY:
    if (handler.writing)
      return nullptr;

    handler.writing = true;
    break;
  case Channel::READ_WRITE:
    if (handler.pending || handler.reading || handler.writing)
      return nullptr;

    m_hasPending = true;
    handler.pending = true;
    handler.writing = true;
    break;
  }

  return std::unique_ptr<IPCEndpoint::Channel>{
    new Channel(shared_from_this(), channel, mode)
  };
}

void IPCEndpoint::ReleaseChannel(uint32_t channel, Channel::Mode mode) {
  if (channel >= Header::NUMBER_OF_CHANNELS) {
    return;
  }
  std::lock_guard<std::mutex> lock(m_pendingMutex);
  auto& handler = m_handler[channel];
  switch (mode) {
  case Channel::WRITE_ONLY:
    handler.writing = false;
    break;
  case Channel::READ_ONLY:
    handler.reading = false;
    handler.pending = false;
    break;
  case Channel::READ_WRITE:
    handler.writing = false;
    handler.reading = false;
    handler.pending = false;
    break;
  }
}

void IPCEndpoint::HandlePendingUnsafe() {
  bool hasPending = false;
  std::lock_guard<std::mutex> pendingLock(m_pendingMutex);
  for (uint32_t channel = 0; channel < Header::NUMBER_OF_CHANNELS; channel++) {
    if (m_handler[channel].pending) {
      // Channel must be in the EOM state when we move it to an actual handler
      if (m_handler[channel].eom) {
        m_handler[channel].reading = true;
        m_handler[channel].pending = false;
        m_handler[channel].eom = false;
      } else {
        hasPending = true; // We still have pending handlers
      }
    }
  }
  m_hasPending = hasPending;
}

std::streamsize IPCEndpoint::Read(uint32_t channel, void* buffer, std::streamsize size, MessageBuffers::SharedBuffer* sharedBuffer) {
  uint8_t* data = reinterpret_cast<uint8_t*>(buffer);
  std::streamsize nRemaining = size;

  if (m_isClosed) {
    return -1;
  }

  // Check to see if we have any pending handlers that can be installed at this time
  if (m_hasPending) {
    HandlePendingUnsafe();
  }

  while (!m_handler[channel].eom && nRemaining > 0) {
    std::unique_lock<std::mutex> lock(m_recvMutex);

    if (m_recvMessage.isProcessingHeader) { // Process Header
      while (m_recvMessage.position < sizeof(Header)) {
        std::streamsize length = ReadRaw((uint8_t*)&m_recvMessage.header + m_recvMessage.position,
                             m_recvMessage.length - m_recvMessage.position);
        if (length <= 0) {
          Close(Reason::ReadFailure);
          return -1;
        }
        m_recvMessage.position += static_cast<uint32_t>(length);
      }
      if (!m_recvMessage.header.Validate()) {
        throw std::runtime_error("Received invalid message header");
      }
      // Skip any extra header content
      const uint32_t headerLength = m_recvMessage.header.Size();
      if (headerLength > sizeof(Header)) {
        const int skipBytes = static_cast<int>(headerLength - sizeof(Header));
        if (ReadRaw(m_drain.data(), skipBytes) != skipBytes) {
          Close(Reason::ReadFailure);
          return -1;
        }
      }
      if (m_hasPending) {
        HandlePendingUnsafe();
      }
      const uint32_t messageChannel = m_recvMessage.header.Channel();
      const bool hasHandler = m_handler[messageChannel].reading;

      // This is an EOM marker
      if (m_recvMessage.header.IsEndOfMessage()) {
        m_handler[messageChannel].eom = true;
      } else if (!hasHandler) {
        // Only if there isn't a handler for a channel will we reset the EOM state
        m_handler[messageChannel].eom = false;
      }

      // Done with header, now handle the payload
      m_recvMessage.BeginPayload();

      if (hasHandler && messageChannel != channel) {
        // This isn't our message, and there is a handler to handle it. Let it do so...
        m_recvCondition.notify_all();
        continue;
      }
    } else if (m_handler[m_recvMessage.header.Channel()].reading) { // Is there a handler for this channel?
      m_recvCondition.wait(lock, [this, channel] {
        const uint32_t messageChannel = m_recvMessage.header.Channel();
        return (messageChannel == channel && m_handler[messageChannel].reading) || m_isClosed;
      });
      if (m_isClosed) {
        m_recvCondition.notify_all(); // Inform any remaining readers that the endpoint has been closed
        return -1;
      }
      if (m_recvMessage.isProcessingHeader) { // Should never be true...
        continue;
      }
    }
    // Process Payload
    const uint32_t messageChannel = m_recvMessage.header.Channel();
    if (messageChannel == channel && m_handler[messageChannel].reading) {
      std::streamsize available = std::min<std::streamsize>(nRemaining, m_recvMessage.length - m_recvMessage.position);
      if (data == nullptr && available > 0) {
        if (sharedBuffer) {
          auto sb = m_sharedBufferPool ?
                    m_sharedBufferPool->Get((size_t)available) : std::make_shared<MessageBuffers::Buffer>((size_t)available);
          if (sb) {
            data = sb->Data();
            if (data) {
              *sharedBuffer = sb;
              nRemaining = size = available;
            }
          }
        }
        if (data == nullptr) {
          throw std::exception(); // We are in big trouble if we still have a null pointer
        }
      }
      while (available > 0) {
        const std::streamsize length = ReadRaw(data, available);
        if (length <= 0) {
          Close(Reason::ReadFailure);
          return -1;
        }
        data += length;
        m_recvMessage.position += static_cast<uint32_t>(length);
        available -= length;
        nRemaining -= length;
      }
    } else {
      // If there isn't a handler, then we will just drain the data
      std::streamsize available = m_recvMessage.length - m_recvMessage.position;
      while (available > 0) {
        const std::streamsize length = ReadRaw(m_drain.data(), std::min<std::streamsize>(static_cast<uint32_t>(m_drain.size()), available));
        if (length <= 0) {
          Close(Reason::ReadFailure);
          return -1;
        }
        available -= length;
        m_recvMessage.position += static_cast<uint32_t>(length);
      }
    }
    // If we have reached the end of the payload, get ready for the next header
    if (m_recvMessage.length == m_recvMessage.position) {
      m_recvMessage.BeginHeader();
    }
    if (m_hasPending) {
      HandlePendingUnsafe();
    }
  }
  return size - nRemaining;
}

MessageBuffers::Buffers IPCEndpoint::ReadMessageBuffers(uint32_t channel) {
  MessageBuffers::Buffers messageBuffers;
  MessageBuffers::SharedBuffer sharedBuffer;
  std::streamsize n;

  while ((n = Read(channel, nullptr, m_blockSize, &sharedBuffer)) >= 0) {
    if (sharedBuffer) {
      if (sharedBuffer->Size() == static_cast<size_t>(n)) { // Make sure that we received all of the data
        messageBuffers.emplace_back(std::move(sharedBuffer));
        sharedBuffer.reset();
      } else {
        throw std::exception();
      }
    }
    if (m_handler[channel].eom) {
      break;
    }
  }
  if (m_isClosed && !m_handler[channel].eom) {
    // If we didn't receive a complete message and we are closed, drop the partial message
    return MessageBuffers::Buffers();
  }
  m_handler[channel].eom = false;
  return messageBuffers;
}

bool IPCEndpoint::WriteMessageBuffers(uint32_t channel, const MessageBuffers::Buffers& messageBuffers) {
  if (messageBuffers.empty()) {
    return false;
  }
  for (size_t i = 0; i < messageBuffers.size(); i++) {
    const auto& sharedBuffer = messageBuffers[i];
    if (!sharedBuffer) {
      continue;
    }
    const auto buffer = sharedBuffer->Data();
    const auto size = sharedBuffer->Size();

    if (!buffer || size <= 0) {
      continue;
    }
    if (!Write(channel, buffer, static_cast<uint32_t>(size), i == messageBuffers.size() - 1)) { // Failed to write, must be closed
      return false;
    }
  }
  return WriteMessageComplete(channel);
}

std::streamsize IPCEndpoint::Read(uint32_t channel, void* buffer, std::streamsize size) {
  return Read(channel, buffer, size, nullptr);
}

bool IPCEndpoint::Write(uint32_t channel, const void* pBuf, std::streamsize nBytes, bool isComplete) {
  const uint8_t* data = reinterpret_cast<const uint8_t*>(pBuf);
  uint64_t nRemaining = nBytes;

  while (nRemaining > 0) {
    std::streamsize available = std::min<std::streamsize>(nRemaining, m_blockSize - sizeof(Header));

    std::lock_guard<std::mutex> lock(m_sendMutex);

    if (m_isClosed) {
      return false;
    }
    if(isComplete)
      m_sendHeader.SetEndOfMessage();
    else
      m_sendHeader.ClearEndOfMessage();
    m_sendHeader.SetChannel(channel);
    m_sendHeader.SetPayloadSize(static_cast<uint32_t>(available));

    if (!WriteRaw(&m_sendHeader, sizeof(m_sendHeader)) ||
        !WriteRaw(data, available)) {
      Close(Reason::WriteFailure);
      return false;
    }
    data += available;
    nRemaining -= available;
  }
  return true;
}

std::streamsize IPCEndpoint::Skip(uint32_t channel, std::streamsize count) {
  // Loop until the number of bytes skipped is the number requested:
  std::streamsize nRemaining = count;
  while (!m_isClosed && !m_handler[channel].eom && nRemaining > 0)
    nRemaining -= Read(channel, m_drain.data(), std::min<std::streamsize>(static_cast<std::streamsize>(m_drain.size()), nRemaining));
  return count - nRemaining;
}

void IPCEndpoint::ReadMessageComplete(uint32_t channel) {
  m_handler[channel].eom = false;
}

bool IPCEndpoint::WriteMessageComplete(uint32_t channel) {
  std::lock_guard<std::mutex> lock(m_sendMutex);

  m_sendHeader.SetEndOfMessage();
  m_sendHeader.SetChannel(channel);
  m_sendHeader.SetPayloadSize(0);

  if (!WriteRaw(&m_sendHeader, sizeof(m_sendHeader))) {
    Close(Reason::WriteFailure);
    return false;
  }
  return true;
}

bool IPCEndpoint::ReadRawN(void* buf, std::streamsize size) {
  uint8_t* pCur = static_cast<uint8_t*>(buf);
  while (size) {
    auto nRead = ReadRaw(pCur, size);
    if(nRead < 0)
      return false;
    pCur += nRead;
    size -= nRead;
  }
  return true;
}

void IPCEndpoint::Close(Reason reason) {
  auto wasClosed = m_isClosed.exchange(true);
  if (!wasClosed)
    // First time we're being closed, notify users
    onConnectionLost(reason);

  m_isClosed = true;
  m_recvCondition.notify_all(); // Inform any remaining readers that the endpoint has been closed
}

const IPCEndpoint::Header& IPCEndpoint::ReadMessageHeader(void) {
  // Skip any bytes remaining:
  if (m_nRemain)
    throw std::runtime_error("Attempted to read a message header when payload bytes remain");

  auto ncb = ReadRaw(&m_lastHeader, sizeof(m_lastHeader));
  if (sizeof(m_lastHeader) == ncb) {
    m_nRemain = m_lastHeader.PayloadSize();
    if (m_lastHeader.magic1 != 0x64 || m_lastHeader.magic2 != 0x37)
      throw std::runtime_error("Magic value error");
  }
  else
    m_lastHeader = {};

  if(sizeof(m_lastHeader) < m_lastHeader.Size()) {
    auto nSkip = m_lastHeader.Size() - sizeof(m_lastHeader);
    if(m_drain.size() < nSkip)
      m_drain.resize(nSkip);
    ReadRawN(m_drain.data(), nSkip);
  }
  return m_lastHeader;
}

std::streamsize IPCEndpoint::ReadPayload(void* pBuf, size_t ncb) {
  // Bounds and trivial return check:
  if (ncb > m_nRemain)
    ncb = m_nRemain;
  if (!ncb)
    return 0;

  auto retVal = ReadRaw(pBuf, ncb);
  if (0 < retVal)
    m_nRemain -= (size_t)retVal;
  return retVal;
}
