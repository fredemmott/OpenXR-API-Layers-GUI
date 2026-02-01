// Copyright 2023 Fred Emmott <fred@fredemmott.com>
// SPDX-License-Identifier: ISC

#include "Linter.hpp"

#include <cassert>
#include <ranges>

namespace FredEmmott::OpenXRLayers {

static std::set<Linter*> gLinters;

static void RegisterLinter(Linter* linter) {
  gLinters.emplace(linter);
}
static void UnregisterLinter(Linter* linter) {
  gLinters.erase(linter);
}

LintError::LintError(
  const std::string& description,
  const LayerKeySet& affectedLayers)
  : mDescription(description),
    mAffectedLayers(affectedLayers) {}

std::string LintError::GetDescription() const {
  return mDescription;
}

LayerKeySet LintError::GetAffectedLayers() const {
  return mAffectedLayers;
}

Linter::Linter() {
  RegisterLinter(this);
}

Linter::~Linter() {
  UnregisterLinter(this);
}

std::vector<std::shared_ptr<LintError>> RunAllLinters(
  const APILayerStore* store,
  const std::vector<APILayer>& layers) {
  std::vector<std::shared_ptr<LintError>> errors;

  std::vector<std::tuple<APILayer, APILayerDetails>> layersWithDetails;
  for (const auto& layer: layers) {
    layersWithDetails.push_back({layer, {layer.mManifestPath}});
  }

  auto it = std::back_inserter(errors);
  for (const auto linter: gLinters) {
    std::ranges::move(linter->Lint(store, layersWithDetails), it);
  }

  return errors;
}

OrderingLintError::OrderingLintError(
  const std::string& description,
  const APILayer& layerToMove,
  Position position,
  const APILayer& relativeTo,
  const LayerKeySet& allAffectedLayers)
  : FixableLintError(
      description,
      allAffectedLayers.empty() ? LayerKeySet {layerToMove, relativeTo}
                                : allAffectedLayers),
    mLayerToMove(layerToMove),
    mPosition(position),
    mRelativeTo(relativeTo) {}

std::vector<APILayer> OrderingLintError::Fix(
  const std::vector<APILayer>& oldLayers) {
  auto newLayers = oldLayers;

  const auto moveIt = std::ranges::find(newLayers, mLayerToMove);

  if (moveIt == newLayers.end()) {
    return oldLayers;
  }
  const auto movedLayer = std::move(*moveIt);
  newLayers.erase(moveIt);

  const auto anchorIt = std::ranges::find(newLayers, mRelativeTo);
  if (anchorIt == newLayers.end()) {
    return oldLayers;
  }

  switch (mPosition) {
    case Position::Above:
      newLayers.insert(anchorIt, movedLayer);
      break;
    case Position::Below:
      newLayers.insert(anchorIt + 1, movedLayer);
      break;
  }

  return newLayers;
}

KnownBadLayerLintError::KnownBadLayerLintError(
  const std::string& description,
  const APILayer& layer)
  : FixableLintError(description, {layer}) {}

std::vector<APILayer> KnownBadLayerLintError::Fix(
  const std::vector<APILayer>& allLayers) {
  const auto affected = this->GetAffectedLayers();
  assert(affected.size() == 1);
  const auto& layer = *affected.begin();

  auto newLayers = allLayers;
  const auto it = std::ranges::find(newLayers, layer);

  if (it != newLayers.end()) {
    it->mValue = APILayer::Value::Disabled;
  }
  return newLayers;
}

InvalidLayerLintError::InvalidLayerLintError(
  const std::string& description,
  const APILayer& layer)
  : FixableLintError(description, {layer}),
    mIsFixable(layer.mValue != APILayer::Value::EnabledButAbsent) {}

bool InvalidLayerLintError::IsFixable() const noexcept {
  return mIsFixable;
}

std::vector<APILayer> InvalidLayerLintError::Fix(
  const std::vector<APILayer>& allLayers) {
  const auto affected = this->GetAffectedLayers();
  assert(affected.size() == 1);
  const auto& key = *affected.begin();

  auto newLayers = allLayers;
  const auto it = std::ranges::find(newLayers, key);

  if (it != newLayers.end()) {
    newLayers.erase(it);
  }
  return newLayers;
}

InvalidLayerStateLintError::InvalidLayerStateLintError(
  const std::string& description,
  const APILayer& layer)
  : FixableLintError(description, {layer}) {}

std::vector<APILayer> InvalidLayerStateLintError::Fix(
  const std::vector<APILayer>& allLayers) {
  const auto affected = this->GetAffectedLayers();
  assert(affected.size() == 1);
  const auto& layer = *affected.begin();

  auto newLayers = allLayers;
  const auto it = std::ranges::find(newLayers, layer);

  if (it != newLayers.end()) {
    it->mValue = APILayer::Value::Disabled;
  }

  return newLayers;
}

}// namespace FredEmmott::OpenXRLayers