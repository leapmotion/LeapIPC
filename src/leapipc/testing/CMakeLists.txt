set(LeapIPCTest_SRCS
  FileMonitorTest.cpp
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

# This is a unit test, let CMake know this
add_test(NAME LeapIPCTest COMMAND $<TARGET_FILE:LeapIPCTest>)
