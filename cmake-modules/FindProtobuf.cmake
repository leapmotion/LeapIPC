include(CreateImportTargetHelpers)

set(_suffix "")
if(APPLE)
  set(_suffix "-libc++")
 endif()

find_path(Protobuf_ROOT_DIR
  NAMES include/google/protobuf/descriptor.h
  PATH_SUFFIXES protobuf-${Protobuf_FIND_VERSION}${_suffix}
                protobuf${_suffix})

find_path(Protobuf_HOST_DIR
  NAMES include/google/protobuf/descriptor.h
  HINTS ${Protobuf_ROOT_DIR}
  PATH_SUFFIXES protobuf-${Protobuf_FIND_VERSION}${_suffix}
                protobuf${_suffix}
  NO_CMAKE_PATH)

set(Protobuf_INCLUDE_DIR ${Protobuf_ROOT_DIR}/include CACHE STRING "")

if(DEFINED Protobuf_LIBRARY AND NOT EXISTS ${Protobuf_LIBRARY})
  unset(Protobuf_LIBRARY CACHE)
endif()

find_library(Protobuf_LIBRARY
  NAMES protobuf libprotobuf #This is dumb, but necessary because on mac, the prefix is "lib", but on windows its ""
  HINTS "${Protobuf_ROOT_DIR}/lib"
        "${Protobuf_ROOT_DIR}/lib/Release"
)

if(WIN32)
  set(Protobuf_LIBRARY_RELEASE ${Protobuf_LIBRARY})
  find_library(Protobuf_LIBRARY_DEBUG
    NAMES protobuf libprotobuf
    HINTS ${Protobuf_ROOT_DIR}/lib/Debug
  )
  mark_as_advanced(Protobuf_LIBRARY_DEBUG)
endif()

find_program(Protobuf_protoc protoc HINTS ${Protobuf_HOST_DIR} PATH_SUFFIXES bin${CROSS_COMPILE_EXE_TYPE} bin NO_CMAKE_PATH)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Protobuf DEFAULT_MSG
                                  Protobuf_LIBRARY Protobuf_INCLUDE_DIR)

if(NOT Protobuf_FOUND)
  return()
endif()

generate_import_target(Protobuf STATIC)

include(CMakeFindDependencyMacro)
find_dependency(ZLIB)
set_property(TARGET Protobuf::Protobuf APPEND PROPERTY INTERFACE_LINK_LIBRARIES ZLIB::ZLIB)

add_executable(Protobuf::protoc IMPORTED)
set_property(TARGET Protobuf::protoc PROPERTY IMPORTED_LOCATION ${Protobuf_protoc})
