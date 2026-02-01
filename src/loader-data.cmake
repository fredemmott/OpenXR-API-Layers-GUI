include(lib.cmake)

add_executable(
  loader-data
  LoaderDataMain.cpp LoaderDataMain.hpp
)

target_link_libraries(
  loader-data
  PRIVATE
  lib # TODO: just 'platform'
  nlohmann_json::nlohmann_json
  OpenXR::headers
  OpenXR::openxr_loader
)
target_include_directories(loader-data PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")

math(EXPR VOID_P_BITS "${CMAKE_SIZEOF_VOID_P} * 8")
set(OUTPUT_NAME "openxr-loader-data-${VOID_P_BITS}")
set_target_properties(
  loader-data
  PROPERTIES
  OUTPUT_NAME "${OUTPUT_NAME}"
  # This is a lie, but makes people less likely to try to run them instead of the main exe
  SUFFIX ".dll"
)
configure_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/version.in.rc"
  "${CODEGEN_BUILD_DIR}/create-report-version.rc"
  @ONLY
)

if (WIN32)
  target_sources(
    loader-data
    PRIVATE
    windows/wWinMain-loader-data.cpp
    "${CODEGEN_BUILD_DIR}/create-report-version.rc"
  )
endif ()