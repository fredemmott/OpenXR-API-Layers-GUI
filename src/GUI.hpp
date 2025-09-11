// Copyright 2023 Fred Emmott <fred@fredemmott.com>
// SPDX-License-Identifier: ISC

#pragma once

#include <vector>

#include "APILayer.hpp"
#include "Linter.hpp"

namespace FredEmmott::OpenXRLayers {
class ReadWriteAPILayerStore;

// The actual app GUI
class GUI final {
 public:
  GUI();
  void Run();

 private:
  using LintErrors = std::vector<std::shared_ptr<LintError>>;

  class LayerSet {
   public:
    std::type_identity_t<const ReadWriteAPILayerStore>* mStore {nullptr};

    std::vector<APILayer> mLayers;
    APILayer* mSelectedLayer {nullptr};
    LintErrors mLintErrors;
    bool mLayerDataIsStale {true};
    bool mLintErrorsAreStale {true};

    bool HasErrors();

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
  };

  std::vector<LayerSet> mLayerSets;

  void Export();
  void DrawFrame();
};

}// namespace FredEmmott::OpenXRLayers
