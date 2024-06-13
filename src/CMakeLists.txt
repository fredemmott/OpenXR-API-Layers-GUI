# Copyright 2023 Fred Emmott <fred@fredemmott.com>
# SPDX-License-Identifier: ISC

find_package(imgui CONFIG REQUIRED)
find_package(ImGui-SFML CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
# Using fmt instead of fmt::format until MacOS/libc++ catch up
find_package(fmt CONFIG REQUIRED)

if(WIN32)
  find_package(cppwinrt CONFIG REQUIRED)
endif()

if(USE_EMOJI)
  set(USE_EMOJI_BOOL "true")
else()
  set(USE_EMOJI_BOOL "false")
endif()

file(READ "${CMAKE_CURRENT_SOURCE_DIR}/../LICENSE" LICENSE_TEXT)

set(CODEGEN_BUILD_DIR "${CMAKE_CURRENT_BINARY_DIR}/${BUILD_TARGET_ID}")
configure_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/Config.in.hpp"
  "${CODEGEN_BUILD_DIR}/Config.hpp"
  @ONLY
)

add_executable(
  gui
  WIN32
  APILayerDetails.cpp
  GUI.cpp
  KnownLayers.cpp
  SaveReport.cpp
  Linter.cpp
  linters/BadInstallationLinter.cpp
  linters/DuplicatesLinter.cpp
  linters/OrderingLinter.cpp
  "${CMAKE_SOURCE_DIR}/icon/icon.rc"
)
install(
  TARGETS gui
  DESTINATION "."
)
sign_target(gui)
# Used by version.in.rc
set(OUTPUT_NAME "OpenXR-API-Layers")
set_target_properties(
  gui
  PROPERTIES
  OUTPUT_NAME "${OUTPUT_NAME}"
)

target_include_directories(
  gui
  PRIVATE
  "${CODEGEN_BUILD_DIR}"
  "${CMAKE_CURRENT_SOURCE_DIR}"
)

target_link_libraries(
  gui
  PRIVATE
  ImGui-SFML::ImGui-SFML
  nlohmann_json::nlohmann_json
  fmt::fmt-header-only
)

set(PLATFORM_APILAYERS_CPP "APILayerStore_stub.cpp")
set(PLATFORM_MAIN_CPP "main.cpp")
set(PLATFORM_GUI_CPP "PlatformGUI_stub.cpp")

if(WIN32)
  set(PLATFORM_APILAYERS_CPP "APILayerStore_windows.cpp")
  set(PLATFORM_GUI_CPP "PlatformGUI_windows.cpp")
  set(PLATFORM_MAIN_CPP "wWinMain.cpp")

  configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/version.in.rc"
    "${CODEGEN_BUILD_DIR}/version.rc"
    @ONLY
  )

  target_sources(
    gui
    PRIVATE
    "${CODEGEN_BUILD_DIR}/version.rc"
    manifest.xml
    linters/windows/NotADWORDLinter.cpp
    linters/windows/OutdatedOpenKneeboardLinter.cpp
    linters/windows/ProgramFilesLinter.cpp
    linters/windows/UnsignedDllLinter.cpp
  )

  target_compile_definitions(
    gui
    PRIVATE
    "WIN32_LEAN_AND_MEAN"
    "NOMINMAX"
  )

  target_compile_options(
    gui
    PRIVATE
    "/EHsc"
    "/diagnostics:caret" 
    "/utf-8"
    "/await:strict"
  )

  target_link_libraries(
    gui
    PRIVATE
    Microsoft::CppWinRT
    Comctl32
    WinTrust
    # Missing dependency for CppWinRT vcpkg
    # https://github.com/microsoft/vcpkg/issues/15339
    RuntimeObject
  )

  target_link_options(
    gui
    PRIVATE
    "/MANIFESTUAC:level='requireAdministrator'"
  )

  install(
    FILES "${CMAKE_SOURCE_DIR}/README-Windows.md"
    RENAME "README-Windows.txt"
    DESTINATION "."
  )
endif()

target_sources(
  gui
  PRIVATE
  "${PLATFORM_APILAYERS_CPP}"
  "${PLATFORM_GUI_CPP}"
  "${PLATFORM_MAIN_CPP}"
)