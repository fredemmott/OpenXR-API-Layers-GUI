// Copyright 2023 Fred Emmott <fred@fredemmott.com>
// SPDX-License-Identifier: ISC

#include <Windows.h>

#include <wil/resource.h>

#include <shellapi.h>

#include "GUI.hpp"
#include "LoaderData.hpp"

// Entrypoint for Windows
//
// See main.cpp for Linux and macOS
int WINAPI wWinMain(
  [[maybe_unused]] HINSTANCE hInstance,
  [[maybe_unused]] HINSTANCE hPrevInstance,
  [[maybe_unused]] PWSTR pCmdLine,
  [[maybe_unused]] int nCmdShow) {
  int argc {};
  const wil::unique_any<LPWSTR*, decltype(&LocalFree), &LocalFree> argv {
    CommandLineToArgvW(GetCommandLineW(), &argc)};

  constexpr std::wstring_view LoaderQuery {L"loader-query"};
  if (argc == 2 && argv.get()[1] == LoaderQuery) {
    FredEmmott::OpenXRLayers::LoaderMain();
    return 0;
  }

  FredEmmott::OpenXRLayers::GUI().Run();

  return 0;
}
