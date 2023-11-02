// Copyright 2023 Fred Emmott <fred@fredemmott.com>
// SPDX-License-Identifier: ISC

#include "GUI.hpp"

namespace FredEmmott::OpenXRLayers {

void PlatformGUI::PlatformInit() {
}

std::vector<std::filesystem::path> PlatformGUI::GetNewAPILayerJSONPaths(
  sf::WindowHandle) {
  return {};
}

void PlatformGUI::SetupFonts(ImGuiIO*) {
  return;
}

}// namespace FredEmmott::OpenXRLayers