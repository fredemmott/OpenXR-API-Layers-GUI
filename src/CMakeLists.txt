# Copyright 2023 Fred Emmott <fred@fredemmott.com>
# SPDX-License-Identifier: ISC

find_package(imgui CONFIG REQUIRED)
find_package(ImGui-SFML CONFIG REQUIRED)

set(MAIN_CPP main.cpp)
set(APILAYERS_CPP APILayers_stub.cpp)

configure_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/Config.in.hpp"
  "${CMAKE_CURRENT_BINARY_DIR}/Config.hpp"
  @ONLY
)

add_executable(
  openxr-layers-gui
  GUI.cpp
  ${MAIN_CPP}
  ${APILAYERS_CPP}
)

target_link_libraries(
  openxr-layers-gui
  PRIVATE
  ImGui-SFML::ImGui-SFML
)

set_target_properties(
  openxr-layers-gui
  PROPERTIES
  OUTPUT_NAME "OpenXR-Layers-${BUILD_TARGET_ID}"
)