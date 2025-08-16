cmake_minimum_required(VERSION 3.29)

################################################################
#  Name of this driver project/program
################################################################
set(SERVER_NAME "contam_server")
set(CLIENT_NAME "contam_client")
set(APP_NAME "main_app")

project(${APP_NAME} LANGUAGES CXX)


################################################################
#  Options
################################################################

option(CLIENT_ONLY "Only build client" OFF)


################################################################
# Dependencies
################################################################

if (NOT CLIENT_ONLY)
  include (contam.cmake)
endif (NOT CLIENT_ONLY)

if(DEFINED ENV{gRPC_DIR})
  message(STATUS "gRPC_DIR: $ENV{gRPC_DIR}")
  list(APPEND CMAKE_PREFIX_PATH $ENV{gRPC_DIR})
endif()

include (grpc.cmake)

find_package(MPI)


################################################################
#  Initialize RPATH
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
#  Protocol buffer for gRPC messages
################################################################

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

# Buil contam_grpc_proto library
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

set_target_properties(contam_grpc_proto PROPERTIES POSITION_INDEPENDENT_CODE TRUE)


################################################################
# Build server target
################################################################

if (NOT CLIENT_ONLY)
  add_executable(${SERVER_NAME}
  	       "${SERVER_NAME}.cpp"
                  utils.cpp
                  contam_driver.cpp
  	)
  
  target_include_directories(${SERVER_NAME} PUBLIC $ENV{CONTAM_INC_DIR})

  target_link_libraries(${SERVER_NAME}
      contam_grpc_proto
      absl::check
      absl::flags
      absl::flags_parse
      absl::log
      absl::log_initialize
      ${_REFLECTION}
      ${_GRPC_GRPCPP}
      ${_PROTOBUF_LIBPROTOBUF} ${CONTAM_LIB})
  
  set_target_properties(${SERVER_NAME} PROPERTIES
  	INSTALL_RPATH $ENV{CONTAM_LIB_DIR})

  list(APPEND INSTALL_TARGETS ${SERVER_NAME})
endif (NOT CLIENT_ONLY)


################################################################
# Build client targets
################################################################

add_library(${CLIENT_NAME} SHARED "${CLIENT_NAME}.cpp" utils.cpp)
add_executable(${APP_NAME} "${APP_NAME}.cpp")

if (MPI_CXX_FOUND)
  target_compile_definitions(${APP_NAME} PRIVATE USE_MPI=1)
endif()

target_link_libraries(${CLIENT_NAME}
    contam_grpc_proto
    absl::check
    absl::flags
    absl::flags_parse
    absl::log
    absl::log_initialize
    ${_REFLECTION}
    ${_GRPC_GRPCPP}
    ${_PROTOBUF_LIBPROTOBUF})

target_link_libraries(${APP_NAME} ${CLIENT_NAME} MPI::MPI_CXX)

set_target_properties(${APP_NAME} PROPERTIES
  BUILD_RPATH "$ORIGIN/../lib"
  INSTALL_RPATH "$ORIGIN/../lib"
)

list(APPEND INSTALL_TARGETS ${APP_NAME} ${CLIENT_NAME})


################################################################
# Install
################################################################

install(TARGETS ${INSTALL_TARGETS}
        ARCHIVE DESTINATION lib
        LIBRARY DESTINATION lib
        PUBLIC_HEADER DESTINATION include)
