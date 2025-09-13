include(lib.cmake)

add_executable(
  gui
  WIN32
  GUI.cpp GUI.hpp
)
target_link_libraries(gui PRIVATE lib linters)

set(OUTPUT_NAME "OpenXR-API-Layers-GUI")
set_target_properties(
  gui
  PROPERTIES
  OUTPUT_NAME "${OUTPUT_NAME}"
)
configure_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/version.in.rc"
  "${CODEGEN_BUILD_DIR}/gui-version.rc"
  @ONLY
)

if (WIN32)
  target_sources(
    gui
    PRIVATE
    "${CMAKE_SOURCE_DIR}/icon/icon.rc"
    "${CODEGEN_BUILD_DIR}/gui-version.rc"
    manifest.xml
    windows/wWinMain-gui.cpp
  )

  target_link_options(
    gui
    PRIVATE
    "/MANIFESTUAC:level='requireAdministrator'"
  )

  set(UPDATER_FILENAME "fredemmott_OpenXR-API-Layers-GUI_Updater.exe")
  add_custom_command(
    TARGET gui POST_BUILD
    COMMAND
    "${CMAKE_COMMAND}" -E copy_if_different
    "${UPDATER_EXE}"
    "$<TARGET_FILE_DIR:gui>/${UPDATER_FILENAME}"
    VERBATIM
  )
endif ()
