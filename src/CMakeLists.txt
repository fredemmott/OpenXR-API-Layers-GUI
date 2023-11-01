# Copyright 2023 Fred Emmott <fred@fredemmott.com>
# SPDX-License-Identifier: ISC

find_package(imgui CONFIG REQUIRED)
find_package(ImGui-SFML CONFIG REQUIRED)

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
  WIN32
  GUI.cpp
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

set(PLATFORM_APILAYERS_CPP "APILayers_stub.cpp")
set(PLATFORM_MAIN_CPP "main.cpp")
set(PLATFORM_GUI_CPP "GUI_stub.cpp")

if(WIN32)
  #set(PLATFORM_APILAYERS_CPP "APILayers_windows.cpp")
  set(PLATFORM_GUI_CPP "GUI_windows.cpp")
  set(PLATFORM_MAIN_CPP "wWinMain.cpp")

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

target_sources(
  openxr-layers-gui
  PRIVATE
  "${PLATFORM_APILAYERS_CPP}"
  "${PLATFORM_GUI_CPP}"
  "${PLATFORM_MAIN_CPP}"
)
