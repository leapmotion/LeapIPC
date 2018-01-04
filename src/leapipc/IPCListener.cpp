// Copyright (C) 2012-2018 Leap Motion, Inc. All rights reserved.
#include "stdafx.h"
#include "IPCListener.h"
#include <iomanip>
#include <random>
#include <sstream>

using namespace leap::ipc;

std::string leap::ipc::IPCTestNamespace(void) {
  std::mt19937 rnd(std::random_device{}());
  std::stringstream randStr;
  randStr
    << std::hex
    << std::setw(8) << rnd() << '-'
    << std::setw(8) << rnd() << '-'
    << std::setw(8) << rnd() << '-'
    << std::setw(8) << rnd();
  return randStr.str();
}

IPCListener::IPCListener(void) :
  CoreThread("IPCListener")
{
}

IPCListener::~IPCListener(void)
{
}
