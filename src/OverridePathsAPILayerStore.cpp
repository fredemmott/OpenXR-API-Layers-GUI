// Copyright 2026 Fred Emmott <fred@fredemmott.com>
// SPDX-License-Identifier: MIT
#include "OverridePathsAPILayerStore.hpp"

#include <ranges>

namespace FredEmmott::OpenXRLayers {

OverridePathsAPILayerStore::OverridePathsAPILayerStore(Platform& platform)
  : mPlatform {platform} {}

OverridePathsAPILayerStore::~OverridePathsAPILayerStore() = default;

APILayer::Kind OverridePathsAPILayerStore::GetKind() const noexcept {
  return APILayer::Kind::Explicit;
}
std::string OverridePathsAPILayerStore::GetDisplayName() const noexcept {
  return "XR_API_LAYER_PATHS";
}

std::vector<APILayer> OverridePathsAPILayerStore::GetAPILayers()
  const noexcept {
  const auto dirs = mPlatform.GetOverridePaths();
  if ((!dirs) || dirs->empty()) {
    return {};
  }
  std::unordered_set<std::string> enabledLayers;
  enabledLayers.insert_range(mPlatform.GetEnabledExplicitAPILayers());

  std::vector<APILayer> ret;

  for (auto&& dir: *dirs) {
    for (auto&& path: std::filesystem::directory_iterator(dir)) {
      if (!path.is_regular_file()) {
        continue;
      }
      // As of 2026-02-01, this case-sensitive check is the same as the OpenXR
      // loader:
      // https://github.com/KhronosGroup/OpenXR-SDK-Source/blob/21714cf0ec46c67f2597d467a34a9004dbf380aa/src/loader/manifest_file.cpp#L80
      if (path.path().extension() != ".json") {
        continue;
      }
      ret.emplace_back(this, path.path(), APILayer::Value::Disabled);
      auto& it = ret.back();
      const APILayerDetails details(it.mManifestPath);

      it.mArchitectures
        = mPlatform.GetSharedLibraryArchitectures(details.mLibraryPath);

      if (enabledLayers.contains(details.mName)) {
        it.mValue = APILayer::Value::Enabled;
      }
    }
  }

  return ret;
}

Architectures OverridePathsAPILayerStore::GetArchitectures() const noexcept {
  return mPlatform.GetArchitectures();
}

}// namespace FredEmmott::OpenXRLayers