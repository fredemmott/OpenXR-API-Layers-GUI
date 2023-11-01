// Copyright 2023 Fred Emmott <fred@fredemmott.com>
// SPDX-License-Identifier: ISC

#include <Windows.h>

#include "GUI.hpp"

// Entrypoint for Windows
//
// See main.cpp for Linux and MacOS
int WINAPI wWinMain(
  [[maybe_unused]] HINSTANCE hInstance,
  [[maybe_unused]] HINSTANCE hPrevInstance,
  [[maybe_unused]] PWSTR pCmdLine,
  [[maybe_unused]] int nCmdShow) {
  FredEmmott::OpenXRLayers::GUI::Run();
  return 0;
}
