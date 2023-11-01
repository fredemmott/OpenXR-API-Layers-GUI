// Copyright 2023 Fred Emmott <fred@fredemmott.com>
// SPDX-License-Identifier: ISC

#include "gui.hpp"

// Entrypoint for Linux and MacOS
//
// See wwinmain.cpp for Windows
int main() {
  FredEmmott::OpenXRLayers::GUI::Run();
  return 0;
}
