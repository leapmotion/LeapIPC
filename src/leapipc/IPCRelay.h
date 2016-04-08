#pragma once
#include <autowiring/CoreThread.h>

/// <summary>
/// Relay type, impelements a CoreThread DispatchQueue
/// </summary>
class IPCRelay:
  public CoreThread
{
public:
  IPCRelay(size_t dispatchCap) {
    SetDispatcherCap(dispatchCap);
  }
};

