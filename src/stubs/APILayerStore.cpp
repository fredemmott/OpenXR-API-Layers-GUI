// Copyright 2023 Fred Emmott <fred@fredemmott.com>
// SPDX-License-Identifier: ISC

#include "APILayerStore.hpp"

namespace FredEmmott::OpenXRLayers {

class StubAPILayerStore final : public APILayerStore {
 public:
  std::string GetDisplayName() const noexcept override {
    return "Stubbed Layers List";
  }

  std::vector<APILayer> GetAPILayers() const noexcept override {
    return {
      APILayer {
        .mJSONPath = {"/var/not_a_real_layer.json"},
        .mValue = APILayer::Value::Enabled,
      },
    };
  }

  bool SetAPILayers(const std::vector<APILayer>&) const noexcept override {
    return false;
  }
};

std::span<const APILayerStore*> APILayerStore::Get() noexcept {
  static const StubAPILayerStore sInstance;
  return {&sInstance};
}

}// namespace FredEmmott::OpenXRLayers