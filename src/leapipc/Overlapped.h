// Copyright (C) 2012-2016 Leap Motion, Inc. All rights reserved.
#pragma once

class Overlapped:
  public OVERLAPPED
{
public:
  Overlapped(void) {
    memset(this, 0, sizeof(*this));
    hEvent = CreateEvent(nullptr, true, false, nullptr);
  }

  Overlapped(Overlapped&& rhs) {
    memset(this, 0, sizeof(*this));
    *this = rhs;
  }

  void operator=(Overlapped&& rhs) {
    std::swap(hEvent, rhs.hEvent);
  }

  ~Overlapped(void) {
    if(hEvent)
      CloseHandle(hEvent);
  }

};
