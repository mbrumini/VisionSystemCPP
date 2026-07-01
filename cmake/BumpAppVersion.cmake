if(NOT DEFINED SOURCE_DIR OR NOT DEFINED BINARY_DIR)
  message(FATAL_ERROR "BumpAppVersion.cmake requires SOURCE_DIR and BINARY_DIR")
endif()

set(CMAKE_SOURCE_DIR "${SOURCE_DIR}")
set(CMAKE_BINARY_DIR "${BINARY_DIR}")

include("${SOURCE_DIR}/cmake/AppVersion.cmake")
vision_generate_app_version(TRUE)
