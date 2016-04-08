set(LeapIPCTest_SRCS
  IPCChannelTest.cpp
  IPCFormatTest.cpp
  IPCListenerTest.cpp
  IPCMessagingTest.cpp
  IPCShutdownTest.cpp
  IPCTestUtils.h
  IPCTestUtils.cpp
)

add_pch(LeapIPCTest_SRCS "stdafx.h" "stdafx.cpp")
add_executable(LeapIPCTest ${LeapIPCTest_SRCS} "${PROJECT_SOURCE_DIR}/src/gtest-all-guard.cpp")
target_link_libraries(LeapIPCTest LeapIPC Autowiring::AutoTesting LeapSerial::LeapSerial)
