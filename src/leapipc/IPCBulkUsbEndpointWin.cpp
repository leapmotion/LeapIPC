// Copyright (C) 2012-2017 Leap Motion, Inc. All rights reserved.
#include "stdafx.h"
#include "IPCBulkUsbEndpointWin.h"
#include <system_error>

using namespace leap::ipc;

IPCBulkUsbEndpointWin::IPCBulkUsbEndpointWin(libusb_device_handle* deviceHandle, uint8_t endpointOut):
 m_DeviceHandle(deviceHandle), m_EndpointOut(endpointOut)
{
}

IPCBulkUsbEndpointWin::~IPCBulkUsbEndpointWin(void)
{
  Abort(Reason::Unspecified);
}

std::streamsize IPCBulkUsbEndpointWin::ReadRaw(void* buffer, std::streamsize size) {
  return 0;
}

bool IPCBulkUsbEndpointWin::WriteRaw(const void* pBuf, std::streamsize nBytes) {
  int transferred = 0;
  status = libusb_bulk_transfer(m_DeviceHandle, m_EndpointOut, (unsigned char*)pBuf, nBytes, &transferred, 50);
  // When a timeout occurs, some data may still have been transferred
  if (status != LIBUSB_SUCCESS && status != LIBUSB_ERROR_TIMEOUT) {
    return false;
  }
  return true;
}

bool IPCBulkUsbEndpointWin::Abort(Reason reason) {
  
}
