include(lib.cmake)

add_executable(create-report WIN32)

target_link_libraries(
  create-report
  PRIVATE
  lib
  linters
)

set(OUTPUT_NAME "OpenXR-API-Layers-Create-Report")
set_target_properties(
  create-report
  PROPERTIES
  OUTPUT_NAME "${OUTPUT_NAME}"
)
configure_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/version.in.rc"
  "${CODEGEN_BUILD_DIR}/create-report-version.rc"
  @ONLY
)

if (WIN32)
  target_sources(
    create-report
    PRIVATE
    "${CMAKE_SOURCE_DIR}/icon/icon.rc"
    windows/wWinMain-create-report.cpp
    "${CODEGEN_BUILD_DIR}/create-report-version.rc"
  )
endif ()