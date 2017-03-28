// Copyright (C) 2012-2017 Leap Motion, Inc. All rights reserved.
#pragma once
#include <windows.h>

namespace leap {
namespace ipc {

template<class T>
class CAutoHandle
{
};

template<>
class CAutoHandle<SC_HANDLE>
{
public:
  CAutoHandle(SC_HANDLE hHnd = nullptr):
    m_hHnd(hHnd)
  {
  }

  void Close()
  {
    if(m_hHnd)
    {
      CloseServiceHandle(m_hHnd);
      m_hHnd = (SC_HANDLE)0;
    }
  }

  ~CAutoHandle(void)
  {
    Close();
  }

private:
  SC_HANDLE m_hHnd;

public:
  CAutoHandle& operator=(SC_HANDLE hHnd)
  {
    Close();
    m_hHnd = hHnd;
    return *this;
  }
  operator SC_HANDLE&(void) {return m_hHnd;}
};

template<>
class CAutoHandle<HANDLE>
{
public:
  CAutoHandle(HANDLE hHnd = nullptr):
    m_hHnd(hHnd)
  {
  }

  void Close()
  {
    if(m_hHnd)
    {
      CloseHandle(m_hHnd);
      m_hHnd = (HANDLE)0;
    }
  }

  ~CAutoHandle(void)
  {
    Close();
  }

private:
  HANDLE m_hHnd;

public:
  CAutoHandle& operator=(HANDLE hHnd)
  {
    Close();
    m_hHnd = hHnd;
    return *this;
  }
  operator HANDLE&(void) { return m_hHnd; }
  operator HANDLE(void) const { return m_hHnd; }
  HANDLE* operator&(void) {return &m_hHnd;}
};

template<>
class CAutoHandle<OVERLAPPED>
{
public :
  CAutoHandle() : m_overlapped({ 0,0,{ 0,0 },CreateEvent(nullptr, false, false, nullptr) }) {}
  ~CAutoHandle() { Close(); }

  CAutoHandle(const CAutoHandle&) = delete;

  void Close() {
    if (m_overlapped.hEvent == nullptr)
      return;

    CloseHandle(m_overlapped.hEvent);
    m_overlapped.hEvent = nullptr;
  }

  operator OVERLAPPED&(void) { return m_overlapped; }

private:
  OVERLAPPED m_overlapped;
};

}
}
