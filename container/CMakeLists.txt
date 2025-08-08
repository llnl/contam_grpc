cmake_minimum_required(VERSION 3.29)

# Name of this driver project/program
set(DRIVER_EXE_NAME "cxLibDriver_testOneZoneWthCtm")
project(${DRIVER_EXE_NAME} VERSION 3.4.1.10 LANGUAGES C)
add_executable(${DRIVER_EXE_NAME} "")

# Source file for example driver
target_sources(cxLibDriver_testOneZoneWthCtm PRIVATE cxLibDriver_testOneZoneWthCtm.c)


# Path to contamx-lib.dll
set(CX_SHARED_LIB_PATH ${CMAKE_CURRENT_SOURCE_DIR}/../..)

# Reference external header files and lib
target_include_directories(${DRIVER_EXE_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../../include/contam-x-inc)
if ( WIN32 )
  target_link_libraries(${DRIVER_EXE_NAME} ${CX_SHARED_LIB_PATH}/contamx-lib.lib)
else()
  target_link_libraries(${DRIVER_EXE_NAME} ${CX_SHARED_LIB_PATH}/libcontamx-lib.so)
endif()

