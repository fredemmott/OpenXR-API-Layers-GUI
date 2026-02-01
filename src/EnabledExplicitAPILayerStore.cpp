// Copyright 2026 Fred Emmott <fred@fredemmott.com>
// SPDX-License-Identifier: MIT

#include "EnabledExplicitAPILayerStore.hpp"

#include <ranges>

namespace FredEmmott::OpenXRLayers {

EnabledExplicitAPILayerStore::EnabledExplicitAPILayerStore(
  Platform& platform,
  const std::vector<APILayerStore*>& backingStores)
  : mPlatform(platform),
    mBackingStores(backingStores) {}

EnabledExplicitAPILayerStore::~EnabledExplicitAPILayerStore() = default;

APILayer::Kind EnabledExplicitAPILayerStore::GetKind() const noexcept {
  return APILayer::Kind::Explicit;
}
std::string EnabledExplicitAPILayerStore::GetDisplayName() const noexcept {
  return "Enabled Explicit";
}

std::vector<APILayer> EnabledExplicitAPILayerStore::GetAPILayers()
  const noexcept {
  std::vector<APILayer> ret;

  const auto installedLayers
    = mBackingStores | std::views::transform([](APILayerStore* store) {
        return store->GetAPILayers();
      })
    | std::views::join | std::views::transform([](const APILayer& layer) {
        return std::tuple {layer, APILayerDetails {layer.mManifestPath}};
      })
    | std::ranges::to<std::vector>();
  for (auto&& name: mPlatform.GetEnabledExplicitAPILayers()) {
    const auto matching
      = std::views::filter(
          installedLayers,
          [&](const auto& pair) { return get<1>(pair).mName == name; })
      | std::ranges::to<std::vector>();
    ret.push_back(
      APILayer::MakeForEnvVar(this, name, APILayer::Value::EnabledButAbsent));
    if (matching.empty()) {
      continue;
    }
    auto& entry = ret.back();
    for (auto&& match: std::views::elements<0>(matching)) {
      if (match.mValue == APILayer::Value::Enabled) {
        entry.mValue = APILayer::Value::Enabled;
        entry.mArchitectures |= match.mArchitectures;
      }
    }
  }
  return ret;
}

Architectures EnabledExplicitAPILayerStore::GetArchitectures() const noexcept {
  Architectures ret {Architecture::Invalid};
  for (auto&& store: mBackingStores) {
    ret |= store->GetArchitectures();
  }
  return ret;
}

}// namespace FredEmmott::OpenXRLayers
