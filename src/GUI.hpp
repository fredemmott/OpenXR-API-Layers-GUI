// Copyright 2023 Fred Emmott <fred@fredemmott.com>
// SPDX-License-Identifier: ISC

#pragma once

#include <portability/filesystem.hpp>
#include <sfml/Window.hpp>

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
  void OpenURI(const std::string& uri);
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
  bool mLintErrorsAreStale {true};

  sf::WindowHandle mWindowHandle {};

  void GUILayersList();

  void GUIButtons();
  void GUIRemoveLayerPopup();

  void GUITabs();
  void GUIErrorsTab();
  void GUIDetailsTab();

  bool GUIHyperlink(const char* text);

  // This should only be called at the top of the frame loop; set
  // mLayerDataIsStale instead.
  void ReloadLayerDataNow();
  // Set mLayerDataIsStale or mLintErrorsAreStale instead
  void RunAllLintersNow();

  void AddLayersClicked();
  void DragDropReorder(const APILayer& source, const APILayer& target);
};

}// namespace FredEmmott::OpenXRLayers
