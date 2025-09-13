include_guard(GLOBAL)

add_library(
  lib
  STATIC
  EXCLUDE_FROM_ALL
  APILayerDetails.cpp
  APILayerSignature.hpp
  ConstexprString.hpp
  GUI.cpp
  LoaderData.cpp LoaderData.hpp
  SaveReport.cpp
  Platform.cpp Platform.hpp
  StringTemplateParameter.hpp
)

# Using an OBJECT library instead of `STATIC`, because otherwise
# Microsoft's linker gets rid of the static initializers used to register linters :(
add_library(
  linters
  OBJECT
  EXCLUDE_FROM_ALL
  Linter.cpp
  LayerRules.cpp
  linters/BadInstallationLinter.cpp
  linters/DuplicatesLinter.cpp
  linters/OrderingLinter.cpp
  linters/DisabledByEnvironmentLinter.cpp
  linters/SkippedByLoaderLinter.cpp
)
target_link_libraries(linters PUBLIC lib)
target_include_directories(
  lib
  PUBLIC
  "${CMAKE_CURRENT_SOURCE_DIR}"
)

target_link_libraries(
  lib
  PUBLIC
  nlohmann_json::nlohmann_json
  fmt::fmt-header-only
  OpenXR::headers
  WIL::WIL
  config-hpp
  imgui::imgui
)

if (WIN32)
  target_sources(
    lib
    PRIVATE
    windows/CheckForUpdates.cpp windows/CheckForUpdates.hpp
    windows/WindowsAPILayerStore.cpp windows/WindowsAPILayerStore.hpp
    windows/WindowsPlatform.cpp windows/WindowsPlatform.hpp
  )

  set(
    WINDOWS_SDK_LIBRARIES
    Comctl32
    RuntimeObject
    Crypt32
    D3d11
    Dxgi
    WinTrust
  )
  foreach (NAME IN LISTS WINDOWS_SDK_LIBRARIES)
    set(LIB_PATH_VAR "${NAME}_PATH}")
    find_library("${LIB_PATH_VAR}" NAMES "${NAME}" REQUIRED)
    target_link_libraries(lib PUBLIC "${${LIB_PATH_VAR}}")
  endforeach ()

  target_sources(
    linters
    PRIVATE
    windows/WindowsAPILayerStore.cpp
    windows/WindowsPlatform.cpp

    linters/windows/NotADWORDLinter.cpp
    linters/windows/OpenXRToolkitLinter.cpp
    linters/windows/OutdatedOpenKneeboardLinter.cpp
    linters/windows/ProgramFilesLinter.cpp
    linters/windows/UnsignedDllLinter.cpp
    linters/windows/XRNeckSaferLinter.cpp
  )

  target_compile_definitions(
    lib
    PUBLIC
    "WIN32_LEAN_AND_MEAN"
    "NOMINMAX"
    "UNICODE"
    "_UNICODE"
  )
  if (MSVC)
    target_compile_options(
      lib
      PUBLIC
      "/EHsc"
      "/diagnostics:caret"
      "/utf-8"
    )
  endif ()
endif ()
