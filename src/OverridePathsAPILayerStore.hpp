// Copyright 2026 Fred Emmott <fred@fredemmott.com>
// SPDX-License-Identifier: MIT
#pragma once

#include "APILayerStore.hpp"
#include "Platform.hpp"

namespace FredEmmott::OpenXRLayers {

class OverridePathsAPILayerStore : public virtual APILayerStore {
 public:
  OverridePathsAPILayerStore() = delete;
  explicit OverridePathsAPILayerStore(Platform& platform);
  ~OverridePathsAPILayerStore() override;

  APILayer::Kind GetKind() const noexcept override;
  std::string GetDisplayName() const noexcept override;
  std::vector<APILayer> GetAPILayers() const noexcept override;
  Architectures GetArchitectures() const noexcept override;

 private:
  Platform& mPlatform;
};
}// namespace FredEmmott::OpenXRLayers
