cmake_minimum_required(VERSION 3.0)
include(version.cmake)
project(LeapIPC VERSION ${LeapIPC_VERSION})

include(CTest)
include(CheckTypeSize)
include("standard/Standard.cmake")

set_property(GLOBAL PROPERTY USE_FOLDERS ON)
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake-modules")
include(AddPCH)
include(ConditionalSources)

# Standard calls
set_standard_output_variables()

# All typically required packages
find_package(autowiring 1.0.2 REQUIRED)
find_package(leapserial 0.4.0 REQUIRED)

# We have unit test projects via googletest, they're added in the places where they are defined
enable_testing()

# Recurse through source directories
include_directories(contrib)
add_subdirectory(contrib)
add_subdirectory(src)

# Now we can generate the version and install stuff
generate_version()
combined_installer(
  VENDOR "Leap Motion"
  CONTACT "cmercenary@gmail.com"
)
