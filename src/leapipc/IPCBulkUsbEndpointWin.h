// Copyright (C) 2012-2017 Leap Motion, Inc. All rights reserved.
#pragma once
#include <fstream>

namespace leap {
namespace ipc {

class IPCBulkUsbEndpointWin:
{
public:
  IPCBulkUsbEndpointWin(libusb_device_handle* deviceHandle, uint8_t endpointOut);
  ~IPCBulkUsbEndpointWin(void);

private:
  libusb_device_handle* m_DeviceHandle = nullptr;
  uint8_t m_EndpointOut = 0xFF;
  

public:
  // IPCEndpoint overrides:
  std::streamsize ReadRaw(void* buffer, std::streamsize size) override;
  bool WriteRaw(const void* pBuf, std::streamsize nBytes) override;
  bool Abort(Reason reason) override;
};

}}
