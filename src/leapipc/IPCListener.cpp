#include "stdafx.h"
#include "IPCListener.h"
#include <iomanip>
#include <random>
#include <sstream>

using namespace leap::ipc;

std::string IPCTestNamespace(void) {
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

IPCListener::IPCListener(void)
{
}

IPCListener::~IPCListener(void)
{
}
