// Copyright 2026 Fred Emmott <fred@fredemmott.com>
// SPDX-License-Identifier: MIT

#include "APILayer.hpp"

#include "APILayerStore.hpp"

namespace FredEmmott::OpenXRLayers {
APILayer::APILayer(
  const APILayerStore* source,
  const std::filesystem::path& manifestPath,
  const Value value)
  : mSource(source),
    mManifestPath(manifestPath),
    mValue(value),
    mKey(manifestPath.string()),
    mArchitectures(source->GetArchitectures()) {}

}// namespace FredEmmott::OpenXRLayers