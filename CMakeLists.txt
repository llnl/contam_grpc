cmake_minimum_required(VERSION 3.29)

# Name of this driver project/program
set(DRIVER_EXE_NAME "cxLibDriver_testOneZoneWthCtm-grpc")
set(CLIENT_EXE_NAME "contam_client")
project(${DRIVER_EXE_NAME} VERSION 3.4.1.10 LANGUAGES CXX)
add_executable(${DRIVER_EXE_NAME} "${DRIVER_EXE_NAME}.cpp")
add_executable(${CLIENT_EXE_NAME} "${CLIENT_EXE_NAME}.cpp")

# Source file for example driver
target_sources(cxLibDriver_testOneZoneWthCtm-grpc PRIVATE cxLibDriver_testOneZoneWthCtm-grpc.cpp)

set (CXLIB_DIR /app/runContamxDll_OneZoneWthCtm/contamx-artifacts/3.4.1.10/contam-x-3.4.1.10-Linux-64bit-cxlibShared)
# Path to contamx-lib.dll
set(CX_SHARED_LIB_PATH ${CXLIB_DIR})


if(DEFINED ENV{gRPC_DIR})
  message(STATUS "gRPC_DIR: $ENV{gRPC_DIR}")
  list(APPEND CMAKE_PREFIX_PATH $ENV{gRPC_DIR})
endif()

include (grpc.cmake)

# Proto file
get_filename_component(contam_proto "proto/contam.proto" ABSOLUTE)
get_filename_component(contam_proto_path "${contam_proto}" PATH)

# Generated sources
set(contam_proto_srcs "${CMAKE_CURRENT_BINARY_DIR}/contam.pb.cc")
set(contam_proto_hdrs "${CMAKE_CURRENT_BINARY_DIR}/contam.pb.h")
set(contam_grpc_srcs "${CMAKE_CURRENT_BINARY_DIR}/contam.grpc.pb.cc")
set(contam_grpc_hdrs "${CMAKE_CURRENT_BINARY_DIR}/contam.grpc.pb.h")
add_custom_command(
      OUTPUT "${contam_proto_srcs}" "${contam_proto_hdrs}" "${contam_grpc_srcs}" "${contam_grpc_hdrs}"
      COMMAND ${_PROTOBUF_PROTOC}
      ARGS --grpc_out "${CMAKE_CURRENT_BINARY_DIR}"
        --cpp_out "${CMAKE_CURRENT_BINARY_DIR}"
        -I "${contam_proto_path}"
        --plugin=protoc-gen-grpc="${_GRPC_CPP_PLUGIN_EXECUTABLE}"
        "${contam_proto}"
      DEPENDS "${contam_proto}")

# Include generated *.pb.h files
include_directories("${CMAKE_CURRENT_BINARY_DIR}")

# contam_grpc_proto
add_library(contam_grpc_proto
  ${contam_grpc_srcs}
  ${contam_grpc_hdrs}
  ${contam_proto_srcs}
  ${contam_proto_hdrs})
target_link_libraries(contam_grpc_proto
  absl::check
  ${_REFLECTION}
  ${_GRPC_GRPCPP}
  ${_PROTOBUF_LIBPROTOBUF})

# Target
target_include_directories(${DRIVER_EXE_NAME} PUBLIC ${CXLIB_DIR}/include/contam-x-inc)
target_link_libraries(${DRIVER_EXE_NAME}
    contam_grpc_proto
    absl::check
    absl::flags
    absl::flags_parse
    absl::log
    absl::log_initialize
    ${_REFLECTION}
    ${_GRPC_GRPCPP}
    ${_PROTOBUF_LIBPROTOBUF} ${CX_SHARED_LIB_PATH}/libcontamx-lib.so)

target_link_libraries(${CLIENT_EXE_NAME}
    contam_grpc_proto
    absl::check
    absl::flags
    absl::flags_parse
    absl::log
    absl::log_initialize
    ${_REFLECTION}
    ${_GRPC_GRPCPP}
    ${_PROTOBUF_LIBPROTOBUF})

set_target_properties(${DRIVER_EXE_NAME} PROPERTIES
    INSTALL_RPATH ${CX_SHARED_LIB_PATH})

################################################################
# Initialize RPATH
################################################################

# Use RPATH on OS X
if (APPLE)
  set(CMAKE_MACOSX_RPATH ON)
endif ()

# Use (i.e. don't skip) RPATH for build
set(CMAKE_SKIP_BUILD_RPATH FALSE)

# Use same RPATH for build and install
set(CMAKE_BUILD_WITH_INSTALL_RPATH FALSE)

# Add the automatically determined parts of the RPATH
# which point to directories outside the build tree to the install RPATH
set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE CACHE BOOL "")

set(CMAKE_INSTALL_RPATH "${CTM_GRPC_INSTALL_LIBDIR}" CACHE STRING "")

list(FIND CMAKE_PLATFORM_IMPLICIT_LINK_DIRECTORIES
    "${CTM_GRPC_INSTALL_LIBDIR}" _IS_SYSTEM_DIR)

if (${_IS_SYSTEM_DIR} STREQUAL "-1")
    # Set the install RPATH correctly
    list(APPEND CMAKE_INSTALL_RPATH
      "${CTM_GRPC_INSTALL_LIBDIR}")
endif ()


################################################################
# Install
################################################################

install(TARGETS ${DRIVER_EXE_NAME} ${CLIENT_EXE_NAME} 
        ARCHIVE DESTINATION lib
        LIBRARY DESTINATION lib
        PUBLIC_HEADER DESTINATION include)
