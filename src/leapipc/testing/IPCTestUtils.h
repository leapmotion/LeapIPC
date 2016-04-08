#pragma once
#include <string>
#include "SystemUtility/Barrier.h"

std::string GenerateNamespaceName(void);

struct TestBarrier {
  TestBarrier(std::size_t recvCount, std::size_t sendCount) :
    acquire(recvCount + sendCount),
    releaseRecv(recvCount),
    releaseSend(sendCount) {}
  Barrier acquire;
  Barrier releaseRecv;
  Barrier releaseSend;
};
