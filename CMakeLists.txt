# Copyright 2023 Fred Emmott <fred@fredemmott.com>
# SPDX-License-Identifier: ISC

cmake_minimum_required(VERSION 3.10)

set(
  CMAKE_TOOLCHAIN_FILE
  "${CMAKE_CURRENT_SOURCE_DIR}/third-party/vcpkg/scripts/buildsystems/vcpkg.cmake"
  CACHE STRING "Vcpkg toolchain file"
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(DEFINED ENV{GITHUB_RUN_NUMBER})
  set(VERSION_BUILD $ENV{GITHUB_RUN_NUMBER})
else()
  set(VERSION_BUILD 0)
endif()


set(
  BUILD_TARGET_ID
  "${BUILD_TARGET_ID}"
  CACHE
  STRING
  "Identifier for the current build, e.g. OS, architecture, HKLM vs HKCU"
)

project(OpenXR-Layers-GUI VERSION 0.0.1.${VERSION_BUILD} LANGUAGES CXX C)

if (APPLE)
	set(BUILD_TARGET_ID "MacOS-${CMAKE_SYSTEM_PROCESSOR}")
else ()
	set(BUILD_TARGET_ID "${CMAKE_SYSTEM_NAME}-${CMAKE_SYSTEM_PROCESSOR}-stub")
endif ()

message(STATUS "Building OpenXR-Layers-GUI ${BUILD_TARGET_ID} v${CMAKE_PROJECT_VERSION}")

add_subdirectory("src")
