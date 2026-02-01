// Copyright 2026 Fred Emmott <fred@fredemmott.com>
// SPDX-License-Identifier: MIT
#pragma once

#include "APILayerStore.hpp"
#include "Platform.hpp"

namespace FredEmmott::OpenXRLayers {

class EnabledExplicitAPILayerStore : public virtual APILayerStore {
 public:
  EnabledExplicitAPILayerStore() = delete;
  explicit EnabledExplicitAPILayerStore(
    Platform& platform,
    const std::vector<APILayerStore*>& backingStores);
  ~EnabledExplicitAPILayerStore() override;

  APILayer::Kind GetKind() const noexcept override;
  std::string GetDisplayName() const noexcept override;
  std::vector<APILayer> GetAPILayers() const noexcept override;
  bool IsForCurrentArchitecture() const noexcept override;

 private:
  Platform& mPlatform;
  std::vector<APILayerStore*> mBackingStores;
};
}// namespace FredEmmott::OpenXRLayers
