# Copyright 2023 Fred Emmott <fred@fredemmott.com>
# SPDX-License-Identifier: ISC

find_package(magic_enum CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(imgui CONFIG REQUIRED)
find_package(OpenXR CONFIG REQUIRED)
# Using fmt instead of fmt::format until MacOS/libc++ catch up
find_package(fmt CONFIG REQUIRED)

if (WIN32)
  find_package(wil CONFIG REQUIRED)
endif ()

if (USE_EMOJI)
  set(USE_EMOJI_BOOL "true")
else ()
  set(USE_EMOJI_BOOL "false")
endif ()

file(READ "${CMAKE_CURRENT_SOURCE_DIR}/../LICENSE" LICENSE_TEXT)

set(CODEGEN_BUILD_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
configure_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/Config.in.hpp"
  "${CODEGEN_BUILD_DIR}/Config.hpp"
  @ONLY
)
add_library(config-hpp INTERFACE "${CODEGEN_BUILD_DIR}/Config.hpp")
target_include_directories(config-hpp INTERFACE "${CODEGEN_BUILD_DIR}")

option(BUILD_EXECUTABLES "Build executables" ON)

option(BUILD_GUI "Build the GUI" "${BUILD_EXECUTABLES}")
if (BUILD_GUI)
  include(gui.cmake)
endif ()

option(BUILD_LOADER_DATA "Build the loader-data executable" "${BUILD_EXECUTABLES}")
if (BUILD_LOADER_DATA)
  include(loader-data.cmake)
endif ()