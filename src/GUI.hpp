// Copyright 2023 Fred Emmott <fred@fredemmott.com>
// SPDX-License-Identifier: ISC

#pragma once

#include <boost/signals2/connection.hpp>

#include <vector>

#include "APILayer.hpp"
#include "Linter.hpp"

namespace FredEmmott::OpenXRLayers {
class ReadWriteAPILayerStore;

// The actual app GUI
class GUI final {
 public:
  GUI();
  ~GUI();
  void Run();

 private:
  using LintErrors = std::vector<std::shared_ptr<LintError>>;

  class LayerSet final {
   public:
    LayerSet() = delete;
    ~LayerSet();
    explicit LayerSet(APILayerStore* store);
    explicit LayerSet(ReadWriteAPILayerStore* store);

    LayerSet(const LayerSet&) = delete;
    LayerSet& operator=(const LayerSet&) = delete;
    LayerSet(LayerSet&&) = default;
    LayerSet& operator=(LayerSet&&) = default;

    [[nodiscard]]
    const APILayerStore& GetStore() const {
      return *mStore;
    }

    [[nodiscard]]
    ReadWriteAPILayerStore& GetReadWriteStore() const {
      if (!mReadWriteStore) [[unlikely]] {
        throw std::logic_error(
          "Attempted to get read-write store from read-only store");
      }
      return *mReadWriteStore;
    }

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

    [[nodiscard]] bool IsReadWrite() const {
      return mReadWriteStore != nullptr;
    }

   private:
    boost::signals2::scoped_connection mOnChangeConnection;

    const APILayerStore* mStore {nullptr};
    ReadWriteAPILayerStore* mReadWriteStore {nullptr};
  };

  std::vector<LayerSet> mLayerSets;
  std::unique_ptr<APILayerStore> mEnabledExplicitLayers {nullptr};

  void Export();
  void DrawFrame();
};

}// namespace FredEmmott::OpenXRLayers
