// Copyright 2023 Fred Emmott <fred@fredemmott.com>
// SPDX-License-Identifier: ISC

#pragma once

#include <sfml/Window.hpp>

#include <filesystem>
#include <unordered_map>
#include <vector>

#include <imgui.h>

#include "APILayer.hpp"
#include "Linter.hpp"

namespace FredEmmott::OpenXRLayers {

// Platform-specific functions implemented in PlatformGUI_P*.cpp
class PlatformGUI {
 protected:
  void PlatformInit();
  std::vector<std::filesystem::path> GetNewAPILayerJSONPaths(
    sf::WindowHandle parent);
  void SetupFonts(ImGuiIO*);
};

// The actual app GUI
class GUI final : public PlatformGUI {
 public:
  void Run();

 private:
  using LintErrors = std::vector<std::shared_ptr<LintError>>;

  std::vector<APILayer> mLayers;
  APILayer* mSelectedLayer {nullptr};
  LintErrors mLintErrors;
  std::unordered_map<APILayer*, LintErrors> mLintErrorsByLayer;
  bool mLayerDataIsStale {true};

  sf::WindowHandle mWindowHandle {};

  void GUILayersList();
  void GUIButtons();
  void GUITabs();
  void GUIErrorsTab();
  void GUIDetailsTab();
};

}// namespace FredEmmott::OpenXRLayers
