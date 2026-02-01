// Copyright 2023 Fred Emmott <fred@fredemmott.com>
// SPDX-License-Identifier: ISC

#include <Windows.h>

#include <wil/resource.h>

#include <ranges>

#include <shellapi.h>

#include "GUI.hpp"

// Entrypoint for Windows
int WINAPI wWinMain(
  [[maybe_unused]] HINSTANCE hInstance,
  [[maybe_unused]] HINSTANCE hPrevInstance,
  [[maybe_unused]] PWSTR pCmdLine,
  [[maybe_unused]] int nCmdShow) {
  using FredEmmott::OpenXRLayers::GUI;
  auto showExplicit {GUI::ShowExplicit::OnlyIfUsed};

  // pCmdLine varies in whether it includes argv[0]
  const auto commandLine = GetCommandLineW();
  int argc {};
  auto argv = CommandLineToArgvW(commandLine, &argc);
  const auto freeArgv
    = wil::scope_exit([&argv]() noexcept { LocalFree(argv); });

  const auto args = std::views::counted(argv, argc)
    | std::views::transform([](auto x) { return std::wstring_view {x}; });
  if (std::ranges::contains(args, L"--show-explicit")) {
    showExplicit = GUI::ShowExplicit::Always;
  }
  GUI(showExplicit).Run();

  return 0;
}
