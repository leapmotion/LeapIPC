set(IPC_SRCS
  FileMonitor.h
  IPCClient.h
  IPCClientConnector.h
  IPCClientConnector.cpp
  IPCEndpoint.h
  IPCEndpoint.cpp
  IPCListener.h
  IPCListener.cpp
  MessageBuffers.h
)

add_windows_sources(IPC_SRCS
  AutoHandle.h
  DefaultSecurityDescriptor.h
  DefaultSecurityDescriptor.cpp
  FileMonitorWin.h
  FileMonitorWin.cpp
  IPCClientWin.h
  IPCClientWin.cpp
  IPCEndpointWin.h
  IPCEndpointWin.cpp
  IPCListenerWin.h
  IPCListenerWin.cpp
  IPCRelay.h
  NamedPipeWin.h
  NamedPipeWin.cpp
  NamedPipeStatusBlockWin.h
  NamedPipeStatusBlockWin.cpp
  Overlapped.h
)

add_unix_sources(IPC_SRCS
  FileMonitorUnix.h
  FileMonitorUnix.cpp
)

add_mac_sources(IPC_SRCS
  FileMonitorMac.h
  FileMonitorMac.cpp
)

add_posix_sources(IPC_SRCS
  IPCClientUnix.h
  IPCClientUnix.cpp
  IPCEndpointUnix.h
  IPCEndpointUnix.cpp
  IPCListenerUnix.h
  IPCListenerUnix.cpp
)

add_pch(IPC_SRCS "stdafx.h" "stdafx.cpp")

add_library(LeapIPC ${IPC_SRCS})
target_link_libraries(LeapIPC ${IPC_LIBS} Autowiring::Autowiring LeapSerial::LeapSerial)

target_include_directories(
  LeapIPC
  PUBLIC
  "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/src>"
  INTERFACE
  "$<INSTALL_INTERFACE:include>"
)

install_headers(TARGET LeapIPC DESTINATION include/leapipc COMPONENT LeapIPC)
install(TARGETS LeapIPC EXPORT LeapIPCTargets
  DESTINATION lib
  COMPONENT LeapIPC
  CONFIGURATIONS ${CMAKE_CONFIGURATION_TYPES}
)

add_subdirectory(testing)
