# Copyright 2023 Fred Emmott <fred@fredemmott.com>
# SPDX-License-Identifier: ISC

find_package(imgui CONFIG REQUIRED)
find_package(ImGui-SFML CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)

if(USE_EMOJI)
  set(USE_EMOJI_BOOL "true")
else()
  set(USE_EMOJI_BOOL "false")
endif()

macro(add_gui)
  set(OPTIONS USE_HKCU)
  set(ONE_VALUE_ARGS BUILD_TARGET_ID)
  cmake_parse_arguments(ARG "${OPTIONS}" "${ONE_VALUE_ARGS}" "" ${ARGN})
  set(TARGET "${ARG_UNPARSED_ARGUMENTS}")
  set(BUILD_TARGET_ID "${ARG_BUILD_TARGET_ID}")

  if(WIN32)
    if(ARG_USE_HKCU)
      set(APILAYER_HKEY_ROOT "HKEY_CURRENT_USER")
    else()
      set(APILAYER_HKEY_ROOT "HKEY_LOCAL_MACHINE")
    endif()
  endif()

  set(CODEGEN_BUILD_DIR "${CMAKE_CURRENT_BINARY_DIR}/${BUILD_TARGET_ID}")
  configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/Config.in.hpp"
    "${CODEGEN_BUILD_DIR}/Config.hpp"
    @ONLY
  )

  add_executable(
    ${TARGET}
    WIN32
    APILayerDetails.cpp
    GUI.cpp
    KnownLayers.cpp
    Linter.cpp
    linters/BadInstallationLinter.cpp
    linters/OrderingLinter.cpp
  )

  target_include_directories(
    ${TARGET}
    PRIVATE
    "${CODEGEN_BUILD_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}"
  )

  target_link_libraries(
    ${TARGET}
    PRIVATE
    ImGui-SFML::ImGui-SFML
    nlohmann_json::nlohmann_json
  )

  set_target_properties(
    ${TARGET}
    PROPERTIES
    OUTPUT_NAME "OpenXR-Layers-${BUILD_TARGET_ID}"
  )

  set(PLATFORM_APILAYERS_CPP "APILayers_stub.cpp")
  set(PLATFORM_MAIN_CPP "main.cpp")
  set(PLATFORM_GUI_CPP "GUI_stub.cpp")

  if(WIN32)
    set(PLATFORM_APILAYERS_CPP "APILayers_windows.cpp")
    set(PLATFORM_GUI_CPP "GUI_windows.cpp")
    set(PLATFORM_MAIN_CPP "wWinMain.cpp")

    target_sources(
      ${TARGET}
      PRIVATE
      manifest.xml
      linters/windows/ProgramFilesLinter.cpp
    )

    target_compile_definitions(
      ${TARGET}
      PRIVATE
      "WIN32_LEAN_AND_MEAN"
      "NOMINMAX"
    )

    target_compile_options(
      ${TARGET}
      PRIVATE
      "/await:strict"
      "/EHsc"
      "/diagnostics:caret" 
      "/utf-8"
    )

    configure_file(
      "${CMAKE_CURRENT_SOURCE_DIR}/Config_windows.in.hpp"
      "${CODEGEN_BUILD_DIR}/Config_windows.hpp"
      @ONLY
    )
  endif()

  target_sources(
    ${TARGET}
    PRIVATE
    "${PLATFORM_APILAYERS_CPP}"
    "${PLATFORM_GUI_CPP}"
    "${PLATFORM_MAIN_CPP}"
  )
endmacro()

if(APPLE)
  add_gui(gui BUILD_TARGET_ID "MacOS-${CMAKE_SYSTEM_PROCESSOR}")
elseif(WIN32)
  option(USE_HKCU "Use HKCU instead of HKLM" OFF)
  if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(BUILD_ARCH "Win64")
  else()
    set(BUILD_ARCH "Win32")
  endif() 

  add_gui(gui-hklm BUILD_TARGET_ID "${BUILD_ARCH}-HKLM")
  add_gui(gui-hkcu USE_HKCU BUILD_TARGET_ID "${BUILD_ARCH}-HKCU")

  target_link_options(
    gui-hklm
    PRIVATE
    "/MANIFESTUAC:level='requireAdministrator'"
  )
else()
	message(FATAL_ERROR "Unhandled system type")
endif()