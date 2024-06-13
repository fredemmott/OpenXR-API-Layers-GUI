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

class APILayerStore;

struct DPIChangeInfo {
  float mDPIScaling {};
  std::optional<ImVec2> mRecommendedSize;
};

// Platform-specific functions implemented in PlatformGUI_P*.cpp
class PlatformGUI {
 public:
  static PlatformGUI& Get();
  virtual ~PlatformGUI() = default;

  virtual void SetWindow(sf::WindowHandle) = 0;

  virtual std::vector<std::filesystem::path> GetNewAPILayerJSONPaths() = 0;
  virtual std::optional<std::filesystem::path> GetExportFilePath() = 0;
  virtual void SetupFonts(ImGuiIO*) = 0;
  virtual float GetDPIScaling() = 0;
  virtual std::optional<DPIChangeInfo> GetDPIChangeInfo() = 0;

  // Use OS/environment equivalent to Explorer
  virtual void ShowFolderContainingFile(const std::filesystem::path&) = 0;

  PlatformGUI(const PlatformGUI&) = delete;
  PlatformGUI(PlatformGUI&&) = delete;
  PlatformGUI& operator=(const PlatformGUI&) = delete;
  PlatformGUI& operator=(PlatformGUI&&) = delete;

 protected:
  PlatformGUI() = default;
};

// The actual app GUI
class GUI final {
 public:
  void Run();

 private:
  using LintErrors = std::vector<std::shared_ptr<LintError>>;

  class LayerSet {
   public:
    std::type_identity_t<const APILayerStore>* mStore {nullptr};

    std::vector<APILayer> mLayers;
    APILayer* mSelectedLayer {nullptr};
    LintErrors mLintErrors;
    std::unordered_map<const APILayer*, LintErrors> mLintErrorsByLayer;
    bool mLayerDataIsStale {true};
    bool mLintErrorsAreStale {true};

    void Draw();

    void GUILayersList();

    void GUIButtons();
    void GUIRemoveLayerPopup();

    void GUITabs();
    void GUIErrorsTab();
    void GUIDetailsTab();

    // This should only be called at the top of the frame loop; set
    // mLayerDataIsStale instead.
    void ReloadLayerDataNow();
    // Set mLayerDataIsStale or mLintErrorsAreStale instead
    void RunAllLintersNow();

    void AddLayersClicked();
    void DragDropReorder(const APILayer& source, const APILayer& target);

    void Export();
  };

  sf::WindowHandle mWindowHandle {};
};

}// namespace FredEmmott::OpenXRLayers
