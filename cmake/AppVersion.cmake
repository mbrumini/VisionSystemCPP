function(vision_read_build_number build_number_out)
  set(build_number_file "${CMAKE_SOURCE_DIR}/BUILD_NUMBER.txt")
  if(EXISTS "${build_number_file}")
    file(READ "${build_number_file}" build_number)
    string(STRIP "${build_number}" build_number)
  else()
    set(build_number 1000)
  endif()

  if(NOT build_number MATCHES "^[0-9]+$")
    set(build_number 1000)
  endif()

  set(${build_number_out} "${build_number}" PARENT_SCOPE)
endfunction()

function(vision_format_app_version build_number version_out)
  if(build_number GREATER 1000)
    set(${version_out} "0.${build_number}" PARENT_SCOPE)
  else()
    set(${version_out} "0.1" PARENT_SCOPE)
  endif()
endfunction()

function(vision_write_app_version_files build_number version_string)
  file(WRITE "${CMAKE_SOURCE_DIR}/BUILD_NUMBER.txt" "${build_number}\n")
  file(WRITE "${CMAKE_SOURCE_DIR}/VERSION.txt" "${version_string}\n")

  set(generated_header "${CMAKE_BINARY_DIR}/generated/AppVersion.generated.h")
  file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/generated")
  file(WRITE "${generated_header}"
    "#pragma once\n#define VISION_APP_VERSION_STRING \"${version_string}\"\n")
endfunction()

function(vision_generate_app_version bump)
  vision_read_build_number(build_number)
  if(bump)
    math(EXPR build_number "${build_number} + 1")
  endif()

  vision_format_app_version("${build_number}" version_string)
  vision_write_app_version_files("${build_number}" "${version_string}")

  if(bump AND EXISTS "${CMAKE_SOURCE_DIR}/src/app/AppVersion.cpp")
    file(TOUCH "${CMAKE_SOURCE_DIR}/src/app/AppVersion.cpp")
  endif()
endfunction()
