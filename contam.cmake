# Copyright 2026 Lawrence Livermore National Security, LLC.
# See the top-level LICENSE file for details.
#
# SPDX-License-Identifier: BSD-3


if (NOT DEFINED ENV{CONTAM_LIB_DIR} OR "$ENV{CONTAM_LIB_DIR}" STREQUAL "")
  message(FATAL_ERROR "Env var CONTAM_LIB_DIR for contamx-lib path is not defined!")
endif()

if (NOT DEFINED ENV{CONTAM_INC_DIR} OR "$ENV{CONTAM_INC_DIR}" STREQUAL "")
  message(FATAL_ERROR "Env var CONTAM_INC_DIR for contamx header path is not defined!")
endif()


if (EXISTS $ENV{CONTAM_LIB_DIR}/libcontamx-lib.so)
	set(CONTAM_LIB $ENV{CONTAM_LIB_DIR}/libcontamx-lib.so)
elseif (EXISTS $ENV{CONTAM_LIB_DIR}/libcontamx-lib.a)
	set(CONTAM_LIB $ENV{CONTAM_LIB_DIR}/libcontamx-lib.a)
else()
	message(FATAL_ERROR "contamx-lib not found at $ENV{CONTAM_LIB_DIR}!")
endif()

if (NOT EXISTS $ENV{CONTAM_INC_DIR})
	message(FATAL_ERROR "contamx header not found at $ENV{CONTAM_INC_DIR}!")
endif()
