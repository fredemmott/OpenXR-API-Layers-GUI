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
  const std::set<std::filesystem::path>& affectedLayers)
  : mDescription(description), mAffectedLayers(affectedLayers) {
}

std::string LintError::GetDescription() const {
  return mDescription;
}

PathSet LintError::GetAffectedLayers() const {
  return mAffectedLayers;
}

Linter::Linter() {
  RegisterLinter(this);
}

Linter::~Linter() {
  UnregisterLinter(this);
}

std::vector<std::shared_ptr<LintError>> RunAllLinters(
  const std::vector<APILayer>& layers) {
  std::vector<std::shared_ptr<LintError>> errors;

  std::vector<std::tuple<APILayer, APILayerDetails>> layersWithDetails;
  for (const auto& layer: layers) {
    layersWithDetails.push_back({layer, {layer.mJSONPath}});
  }

  auto it = std::back_inserter(errors);
  for (const auto linter: gLinters) {
    std::ranges::move(linter->Lint(layersWithDetails), it);
  }

  return errors;
}

InvalidLayerLintError::InvalidLayerLintError(
  const std::string& description,
  const std::filesystem::path& layer)
  : FixableLintError(description, {layer}) {
}

std::vector<APILayer> InvalidLayerLintError::Fix(
  const std::vector<APILayer>& allLayers) {
  const auto affected = this->GetAffectedLayers();
  assert(affected.size() == 1);
  const auto& path = *affected.begin();

  auto newLayers = allLayers;
  auto it = std::ranges::find_if(
    newLayers, [&path](const auto& layer) { return layer.mJSONPath == path; });

  if (it != newLayers.end()) {
    newLayers.erase(it);
  }
  return newLayers;
}

}// namespace FredEmmott::OpenXRLayers