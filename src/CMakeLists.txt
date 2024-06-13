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
    linters/DuplicatesLinter.cpp
    linters/OrderingLinter.cpp
    "${CMAKE_SOURCE_DIR}/icon/icon.rc"
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
    fmt::fmt-header-only
  )

  set_target_properties(
    ${TARGET}
    PROPERTIES
    OUTPUT_NAME "OpenXR-API-Layers-${BUILD_TARGET_ID}"
  )

  set(PLATFORM_APILAYERS_CPP "APILayers_stub.cpp")
  set(PLATFORM_MAIN_CPP "main.cpp")
  set(PLATFORM_GUI_CPP "PlatformGUI_stub.cpp")
  set(PLATFORM_WATCHER_CPP "Watcher_stub.cpp")

  if(WIN32)
    set(PLATFORM_APILAYERS_CPP "APILayers_windows.cpp")
    set(PLATFORM_GUI_CPP "PlatformGUI_windows.cpp")
    set(PLATFORM_MAIN_CPP "wWinMain.cpp")
    set(PLATFORM_WATCHER_CPP "Watcher_windows.cpp")

    target_sources(
      ${TARGET}
      PRIVATE
      manifest.xml
      linters/windows/NotADWORDLinter.cpp
      linters/windows/OutdatedOpenKneeboardLinter.cpp
      linters/windows/ProgramFilesLinter.cpp
      linters/windows/UnsignedDllLinter.cpp
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
      "/EHsc"
      "/diagnostics:caret" 
      "/utf-8"
      "/await:strict"
    )

    target_link_libraries(
      ${TARGET}
      PRIVATE
      Microsoft::CppWinRT
      Comctl32
      WinTrust
      # Missing dependency for CppWinRT vcpkg
      # https://github.com/microsoft/vcpkg/issues/15339
      RuntimeObject
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
    "${PLATFORM_WATCHER_CPP}"
  )

  install(
    TARGETS "${TARGET}"
    DESTINATION "."
  )

  sign_target("${TARGET}")
endmacro()

if(APPLE)
  add_gui(
    gui
    BUILD_TARGET_ID "MacOS-${CMAKE_SYSTEM_PROCESSOR}"
  )
elseif(WIN32)
  option(USE_HKCU "Use HKCU instead of HKLM" OFF)
  if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(BUILD_ARCH "Win64")
  else()
    set(BUILD_ARCH "Win32")
  endif()
  message(STATUS "Building for ${BUILD_ARCH} with ${CMAKE_SIZEOF_VOID_P}-byte pointers")

  # Using BUILD_ARCH instead of just "Windows" as we need separate
  # DLLs for the 32- and 64-bit versions

  add_gui(
    gui-hklm
    BUILD_TARGET_ID "${BUILD_ARCH}-HKLM"
  )

  add_gui(
    gui-hkcu
    USE_HKCU
    BUILD_TARGET_ID "${BUILD_ARCH}-HKCU"
  )

  target_link_options(
    gui-hklm
    PRIVATE
    "/MANIFESTUAC:level='requireAdministrator'"
  )

  install(
    FILES "${CMAKE_SOURCE_DIR}/README-Windows.md"
    RENAME "README-Windows.txt"
    DESTINATION "."
  )
else()
	message(FATAL_ERROR "Unhandled system type")
endif()
