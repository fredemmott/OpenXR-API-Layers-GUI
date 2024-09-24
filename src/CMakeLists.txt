# Copyright 2023 Fred Emmott <fred@fredemmott.com>
# SPDX-License-Identifier: ISC

find_package(imgui CONFIG REQUIRED)
find_package(ImGui-SFML CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
# Using fmt instead of fmt::format until MacOS/libc++ catch up
find_package(fmt CONFIG REQUIRED)

if(WIN32)
  find_package(cppwinrt CONFIG REQUIRED)
  find_package(wil CONFIG REQUIRED)
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

add_library(
  lib
  STATIC
  APILayerDetails.cpp
  GUI.cpp
  KnownLayers.cpp
  SaveReport.cpp
  std23/ranges.hpp
  ConstexprString.hpp
  StringTemplateParameter.hpp
)

# Using an OBJECT library instead of `STATIC`, because otherwise
# Microsoft's linker gets rid of the static initializers used to register linters :(
add_library(
  linters
  OBJECT
  Linter.cpp
  linters/BadInstallationLinter.cpp
  linters/DuplicatesLinter.cpp
  linters/OrderingLinter.cpp
)
target_link_libraries(linters PUBLIC lib)
target_include_directories(
  lib
  PUBLIC
  "${CODEGEN_BUILD_DIR}"
  "${CMAKE_CURRENT_SOURCE_DIR}"
)

target_link_libraries(
  lib
  PUBLIC
  ImGui-SFML::ImGui-SFML
  nlohmann_json::nlohmann_json
  fmt::fmt-header-only
)

add_executable(
  gui
  WIN32
  "${CMAKE_SOURCE_DIR}/icon/icon.rc"
)
install(
  TARGETS gui
  DESTINATION "."
)
sign_target(gui)
# Used by version.in.rc
set(OUTPUT_NAME "OpenXR-API-Layers-GUI")
set_target_properties(
  gui
  PROPERTIES
  OUTPUT_NAME "${OUTPUT_NAME}"
)
target_link_libraries(gui PRIVATE lib linters)

set(PLATFORM_APILAYERS_CPP "stubs/APILayerStore.cpp")
set(PLATFORM_MAIN_CPP "main.cpp")
set(PLATFORM_GUI_CPP "stubs/PlatformGUI.cpp")
set(PLATFORM_REPORT_MAIN_CPP "SaveReport_main.cpp")

if(WIN32)
  set(PLATFORM_APILAYERS_CPP "windows/WindowsAPILayerStore.cpp")
  set(PLATFORM_GUI_CPP "windows/PlatformGUI.cpp")
  set(PLATFORM_MAIN_CPP "windows/wWinMain.cpp")
  set(PLATFORM_REPORT_MAIN_CPP "windows/SaveReport_wWinMain.cpp")

  configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/version.in.rc"
    "${CODEGEN_BUILD_DIR}/gui-version.rc"
    @ONLY
  )

  target_sources(
    lib
    PRIVATE
    windows/CheckForUpdates.cpp
  )

  target_sources(
    linters
    PRIVATE
    linters/windows/NotADWORDLinter.cpp
    linters/windows/OutdatedOpenKneeboardLinter.cpp
    linters/windows/ProgramFilesLinter.cpp
    linters/windows/UnsignedDllLinter.cpp
  )

  target_sources(
    gui
    PRIVATE
    "${CODEGEN_BUILD_DIR}/gui-version.rc"
    manifest.xml
  )

  target_compile_definitions(
    lib
    PUBLIC
    "WIN32_LEAN_AND_MEAN"
    "NOMINMAX"
  )

  target_compile_options(
    lib
    PUBLIC
    "/EHsc"
    "/diagnostics:caret" 
    "/utf-8"
    "/await:strict"
  )

  target_link_libraries(
    lib
    PUBLIC
    Microsoft::CppWinRT
    WIL::WIL
    Comctl32
    WinTrust
    Dwmapi
    # Missing dependency for CppWinRT vcpkg
    # https://github.com/microsoft/vcpkg/issues/15339
    RuntimeObject
  )

  target_link_options(
    gui
    PRIVATE
    "/MANIFESTUAC:level='requireAdministrator'"
  )

endif()

target_sources(
  lib
  PRIVATE
  "${PLATFORM_APILAYERS_CPP}"
  "${PLATFORM_GUI_CPP}"
)
target_sources(
  gui
  PRIVATE
  "${PLATFORM_MAIN_CPP}"
)

set(OUTPUT_NAME "OpenXR-API-Layers-Create-Report")
configure_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/version.in.rc"
  "${CODEGEN_BUILD_DIR}/create-report-version.rc"
  @ONLY
)
add_executable(
  create-report 
  WIN32
  "${PLATFORM_REPORT_MAIN_CPP}"
  "${CMAKE_SOURCE_DIR}/icon/icon.rc"
  "${CODEGEN_BUILD_DIR}/create-report-version.rc"
)
target_link_libraries(
  create-report
  PRIVATE
  lib
  linters
)
set_target_properties(
  create-report
  PROPERTIES
  OUTPUT_NAME "${OUTPUT_NAME}"
)
sign_target(create-report)
install(
  TARGETS create-report
  DESTINATION "."
)

if (WIN32)
  set(UPDATER_FILENAME "fredemmott_OpenXR-API-Layers-GUI_Updater.exe")
  add_custom_command(
    TARGET gui POST_BUILD
    COMMAND
    "${CMAKE_COMMAND}" -E copy_if_different
    "${UPDATER_EXE}"
    "$<TARGET_FILE_DIR:gui>/${UPDATER_FILENAME}"
    VERBATIM
  )
  install(
    FILES
    "$<TARGET_FILE_DIR:gui>/${UPDATER_FILENAME}"
    DESTINATION "."
  )

  install(
    FILES
    "$<TARGET_PDB_FILE:create-report>"
    "$<TARGET_PDB_FILE:gui>"
    DESTINATION "."
    COMPONENT "DebugSymbols"
    EXCLUDE_FROM_ALL
  )
endif()