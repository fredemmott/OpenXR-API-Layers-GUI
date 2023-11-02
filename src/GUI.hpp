// Copyright 2023 Fred Emmott <fred@fredemmott.com>
// SPDX-License-Identifier: ISC

#pragma once

#include <sfml/Window.hpp>

#include <filesystem>
#include <vector>

#include <imgui.h>

namespace FredEmmott::OpenXRLayers::GUI {

void Run();

void PlatformInit();
std::vector<std::filesystem::path> GetNewAPILayerJSONPaths(
  sf::WindowHandle parent);
void SetupFonts(ImGuiIO*);

}// namespace FredEmmott::OpenXRLayers::GUI
