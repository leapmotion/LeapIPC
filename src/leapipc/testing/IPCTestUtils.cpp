#include "stdafx.h"
#include "IPCTestUtils.h"
#include <cstdlib>
#include <ctime>
#if !defined(_MSC_VER)
#include <unistd.h>
#endif

std::string GenerateNamespaceName(void) {
  std::string retVal;

  // Seed rand
  static bool seed_rand = true;
  if (seed_rand) {
    std::srand(static_cast<unsigned int>(std::time(0)));
    seed_rand = false;
  }

  // Random 32-digit string for the namespace name.  This prevents tests from interfering
  // with each other because they are using the same namespace, and one test is leaking
  // into another test.
  const char table [] = "0123456789ABCDEF";
  for(size_t i = 32; i--;)
    retVal.push_back(table[rand() % 16]);
  return retVal;
}
