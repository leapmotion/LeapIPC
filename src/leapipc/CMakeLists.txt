set(IPC_SRCS
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

add_posix_sources(IPC_SRCS
  IPCClientUnix.h
  IPCClientUnix.cpp
  IPCEndpointUnix.h
  IPCEndpointUnix.cpp
  IPCListenerUnix.h
  IPCListenerUnix.cpp
)

add_pch(IPC_SRCS "stdafx.h" "stdafx.cpp")

find_package(autowiring 0.7.8 REQUIRED)
find_package(leapserial REQUIRED)

add_library(LeapIPC ${IPC_SRCS})
target_link_libraries(LeapIPC ${IPC_LIBS} Autowiring::Autowiring LeapSerial::LeapSerial)

if(NOT WIN32)
  find_package(Boost REQUIRED COMPONENTS filesystem)

  set(Boost_FILESYSTEM_LIBRARY libboost_filesystem)
  add_library(${Boost_FILESYSTEM_LIBRARY} STATIC IMPORTED)
  set_property(TARGET ${Boost_FILESYSTEM_LIBRARY} PROPERTY IMPORTED_LOCATION ${Boost_LIBRARY_DIRS}/${Boost_FILESYSTEM_LIBRARY}${STATIC_LIBRARY_SUFFIX})

  set(Boost_SYSTEM_LIBRARY libboost_system)
  add_library(${Boost_SYSTEM_LIBRARY} STATIC IMPORTED)
  set_property(TARGET ${Boost_SYSTEM_LIBRARY} PROPERTY IMPORTED_LOCATION ${Boost_LIBRARY_DIRS}/${Boost_SYSTEM_LIBRARY}${STATIC_LIBRARY_SUFFIX})

  target_link_libraries(LeapIPC ${Boost_FILESYSTEM_LIBRARY} ${Boost_SYSTEM_LIBRARY})
endif()

install_headers(TARGET LeapIPC DESTINATION include/leapipc COMPONENT LeapIPC)
install(TARGETS LeapIPC EXPORT LeapIPCTargets
  DESTINATION lib
  COMPONENT LeapIPC
  CONFIGURATIONS ${CMAKE_CONFIGURATION_TYPES}
)

add_subdirectory(testing)
