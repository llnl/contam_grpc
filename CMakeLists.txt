cmake_minimum_required(VERSION 3.22)

################################################################
#  Name of this driver project/program
################################################################
set(SERVER_NAME "contam_server")
set(CLIENT_NAME "contam_client")
set(APP_NAME "contam_grpc")

set(PRJ_NAME "contam_grpc")
project(${PRJ_NAME} LANGUAGES CXX)

# Define project version
set(PRJ_VERSION_MAJOR 0)
set(PRJ_VERSION_MINOR 1)
set(PRJ_VERSION_PATCH 0)



################################################################
#  Options
################################################################

option(CLIENT_ONLY "Only build client" OFF)
option(MPI_CLIENT "Client uses MPI" OFF)


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

include(grpc.cmake)

# For the client example that runs with MPI
if (MPI_CLIENT)
  find_package(MPI)
endif()


################################################################
#  Initialize RPATH
################################################################

# Will set global variables such as CMAKE_INSTALL_LIBDIR
include(GNUInstallDirs)

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

set(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}" CACHE STRING "")

list(FIND CMAKE_PLATFORM_IMPLICIT_LINK_DIRECTORIES
  "${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}" _IS_SYSTEM_DIR)

if (${_IS_SYSTEM_DIR} STREQUAL "-1")
  # Set the install RPATH correctly
  list(APPEND CMAKE_INSTALL_RPATH
  "${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}")
endif ()


#################################################################
#  Protocol buffer for contam_ambience and contam_params messages
#################################################################

get_filename_component(contam_ambience_proto "proto/contam_ambience.proto" ABSOLUTE)
get_filename_component(contam_ambience_proto_path "${contam_ambience_proto}" PATH)

# Generated sources
set(contam_ambience_proto_srcs "${CMAKE_CURRENT_BINARY_DIR}/contam_ambience.pb.cc")
set(contam_ambience_proto_hdrs "${CMAKE_CURRENT_BINARY_DIR}/contam_ambience.pb.h")
add_custom_command(
  OUTPUT "${contam_ambience_proto_srcs}" "${contam_ambience_proto_hdrs}"
  COMMAND ${_PROTOBUF_PROTOC}
  ARGS --cpp_out "${CMAKE_CURRENT_BINARY_DIR}"
       -I "${contam_ambience_proto_path}"
       "${contam_ambience_proto}"
  DEPENDS "${contam_ambience_proto}")

get_filename_component(contam_params_proto "proto/contam_params.proto" ABSOLUTE)
get_filename_component(contam_params_proto_path "${contam_params_proto}" PATH)

# Generated sources
set(contam_params_proto_srcs "${CMAKE_CURRENT_BINARY_DIR}/contam_params.pb.cc")
set(contam_params_proto_hdrs "${CMAKE_CURRENT_BINARY_DIR}/contam_params.pb.h")
add_custom_command(
  OUTPUT "${contam_params_proto_srcs}" "${contam_params_proto_hdrs}"
  COMMAND ${_PROTOBUF_PROTOC}
  ARGS --cpp_out "${CMAKE_CURRENT_BINARY_DIR}"
       -I "${contam_params_proto_path}"
       "${contam_params_proto}"
  DEPENDS "${contam_params_proto}")


################################################################
#  Protocol buffer for gRPC messages
################################################################

get_filename_component(contam_rpc_proto "proto/contam_rpc.proto" ABSOLUTE)
get_filename_component(contam_rpc_proto_path "${contam_rpc_proto}" PATH)

# Generated sources
set(contam_rpc_proto_srcs "${CMAKE_CURRENT_BINARY_DIR}/contam_rpc.pb.cc")
set(contam_rpc_proto_hdrs "${CMAKE_CURRENT_BINARY_DIR}/contam_rpc.pb.h")
set(contam_grpc_srcs "${CMAKE_CURRENT_BINARY_DIR}/contam_rpc.grpc.pb.cc")
set(contam_grpc_hdrs "${CMAKE_CURRENT_BINARY_DIR}/contam_rpc.grpc.pb.h")
add_custom_command(
  OUTPUT "${contam_rpc_proto_srcs}" "${contam_rpc_proto_hdrs}"
         "${contam_grpc_srcs}" "${contam_grpc_hdrs}"
  COMMAND ${_PROTOBUF_PROTOC}
  ARGS --grpc_out "${CMAKE_CURRENT_BINARY_DIR}"
       --cpp_out "${CMAKE_CURRENT_BINARY_DIR}"
       -I "${contam_rpc_proto_path}"
       --plugin=protoc-gen-grpc="${_GRPC_CPP_PLUGIN_EXECUTABLE}"
       "${contam_rpc_proto}"
  DEPENDS "${contam_rpc_proto}")

# Include generated *.pb.h files
include_directories("${CMAKE_CURRENT_BINARY_DIR}")


################################################################
#  Build contam_grpc_proto library
################################################################

add_library(contam_grpc_proto STATIC
  ${contam_grpc_srcs}
  ${contam_grpc_hdrs}
  ${contam_rpc_proto_srcs}
  ${contam_rpc_proto_hdrs}
  ${contam_params_proto_srcs}
  ${contam_params_proto_hdrs}
  ${contam_ambience_proto_srcs}
  ${contam_ambience_proto_hdrs})

target_link_libraries(contam_grpc_proto PRIVATE
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
    contam_ambience.cpp
    contam_driver.cpp)

  target_include_directories(${SERVER_NAME} PUBLIC $ENV{CONTAM_INC_DIR})

  target_link_libraries(${SERVER_NAME} PRIVATE
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

  # Define the BUILD_DEBUG macro only for the Debug build type
  target_compile_definitions(${SERVER_NAME} PRIVATE
    $<$<CONFIG:Debug>:BUILD_DEBUG>)

  list(APPEND PRJ_INSTALL_TARGETS ${SERVER_NAME})
endif (NOT CLIENT_ONLY)


################################################################
# Build client library targets
################################################################

add_library(${CLIENT_NAME} SHARED
           "${CLIENT_NAME}.cpp"
            contam_params.cpp
            contam_ambience.cpp
            utils.cpp)

if (CLIENT_ONLY)
  target_compile_definitions(${CLIENT_NAME} PRIVATE CLIENT_ONLY=1)
endif()

target_link_libraries(${CLIENT_NAME} PRIVATE
  contam_grpc_proto
  absl::check
  absl::flags
  absl::flags_parse
  absl::log
  absl::log_initialize
  ${_REFLECTION}
  ${_GRPC_GRPCPP}
  ${_PROTOBUF_LIBPROTOBUF})

target_include_directories(${CLIENT_NAME}
  PUBLIC
  $ENV{CONTAM_INC_DIR}
  "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>"
  "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>")

set_target_properties(${CLIENT_NAME} PROPERTIES PUBLIC_HEADER contam_client.h)

#set_target_properties(${CLIENT_NAME} PROPERTIES
#  BUILD_RPATH "$ORIGIN"
#  INSTALL_RPATH "$ORIGIN")

# Define the BUILD_DEBUG macro only for the Debug build type
target_compile_definitions(${CLIENT_NAME} PRIVATE
  $<$<CONFIG:Debug>:BUILD_DEBUG>)

if (gRPC_INSTALL)
  list(APPEND CONTAM_CLIENT_TARGETS
    ${CLIENT_NAME})
#    contam_grpc_proto
#    ${_REFLECTION}
#    ${_GRPC_GRPCPP})
else()
  list(APPEND CONTAM_CLIENT_TARGETS
    ${CLIENT_NAME})
endif()

################################################################
# Build client user application target
################################################################

add_executable(${APP_NAME} "${APP_NAME}.cpp")

if (MPI_CXX_FOUND)
  target_compile_definitions(${APP_NAME} PRIVATE USE_MPI=1)
  target_link_libraries(${APP_NAME} ${CLIENT_NAME} MPI::MPI_CXX)
else()
  target_link_libraries(${APP_NAME} ${CLIENT_NAME})
endif()

set_target_properties(${APP_NAME} PROPERTIES
  BUILD_RPATH "$ORIGIN/../${CMAKE_INSTALL_LIBDIR}"
  INSTALL_RPATH "$ORIGIN/../${CMAKE_INSTALL_LIBDIR}")

list(APPEND PRJ_INSTALL_TARGETS ${APP_NAME})


################################################################
# Install
################################################################

install(TARGETS ${PRJ_INSTALL_TARGETS}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
        PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

install(TARGETS ${CONTAM_CLIENT_TARGETS}
        EXPORT ContamClientTargets
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
        PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

install(EXPORT ContamClientTargets
        FILE ContamClientTargets.cmake
        NAMESPACE ContamClient::
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/ContamClient)


################################################################
# Export contam_client libray as package
################################################################

include(CMakePackageConfigHelpers)

# Construct the full version string
set(PRJ_VERSION "${PRJ_VERSION_MAJOR}.${PRJ_VERSION_MINOR}.${PRJ_VERSION_PATCH}")

# Define the output path for the version file
set(PRJ_VERSION_FILE_PATH "${CMAKE_CURRENT_BINARY_DIR}/ContamClientConfigVersion.cmake")

# Write the basic package version file
write_basic_package_version_file(
  "${PRJ_VERSION_FILE_PATH}"
  VERSION "${PRJ_VERSION}"
  COMPATIBILITY AnyNewerVersion)

configure_package_config_file(${CMAKE_CURRENT_SOURCE_DIR}/cmake/ContamClientConfig.cmake.in
  "${CMAKE_CURRENT_BINARY_DIR}/ContamClientConfig.cmake"
  INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/ContamClient)

install(FILES
          "${CMAKE_CURRENT_BINARY_DIR}/ContamClientConfig.cmake"
          "${CMAKE_CURRENT_BINARY_DIR}/ContamClientConfigVersion.cmake"
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/ContamClient
)
