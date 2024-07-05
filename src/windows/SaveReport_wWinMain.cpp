// Copyright 2023 Fred Emmott <fred@fredemmott.com>
// SPDX-License-Identifier: ISC

#include <Windows.h>

#include "GUI.hpp"
#include "SaveReport.hpp"

using namespace FredEmmott::OpenXRLayers;

// Entrypoint for Windows
//
// See main.cpp for Linux and MacOS
int WINAPI wWinMain(
  [[maybe_unused]] HINSTANCE hInstance,
  [[maybe_unused]] HINSTANCE hPrevInstance,
  [[maybe_unused]] PWSTR pCmdLine,
  [[maybe_unused]] int nCmdShow) {
  // Make the file picker high-DPI if supported
  SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

  auto& gui = PlatformGUI::Get();
  const auto path = gui.GetExportFilePath();
  if (!path) {
    return 0;
  }

  SaveReport(*path);

  if (std::filesystem::exists(*path)) {
    gui.ShowFolderContainingFile(*path);
  }

  return 0;
}
