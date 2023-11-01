# Copyright 2023 Fred Emmott <fred@fredemmott.com>
# SPDX-License-Identifier: ISC

find_package(imgui CONFIG REQUIRED)
find_package(ImGui-SFML CONFIG REQUIRED)

set(MAIN_CPP main.cpp)
set(APILAYERS_CPP APILayers_stub.cpp)

if(USE_EMOJI)
  set(USE_EMOJI_BOOL "true")
else()
  set(USE_EMOJI_BOOL "false")
endif()

set(CODEGEN_PATH "${CMAKE_CURRENT_BINARY_DIR}/${BUILD_TARGET_ID}")
configure_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/Config.in.hpp"
  "${CODEGEN_PATH}/Config.hpp"
  @ONLY
)

add_executable(
  openxr-layers-gui
  GUI.cpp
  ${MAIN_CPP}
  ${APILAYERS_CPP}
)

target_include_directories(
  openxr-layers-gui
  PRIVATE
  "${CODEGEN_PATH}"
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

if(WIN32)
  target_sources(
    openxr-layers-gui
    PRIVATE
    GUI_windows.cpp
  )

  target_compile_definitions(
    openxr-layers-gui
    PRIVATE
    "WIN32_LEAN_AND_MEAN"
    "NOMINMAX"
  )

  target_compile_options(
    openxr-layers-gui
    PRIVATE
    "/await:strict"
    "/EHsc"
    "/diagnostics:caret" 
    "/utf-8"
  )
endif()