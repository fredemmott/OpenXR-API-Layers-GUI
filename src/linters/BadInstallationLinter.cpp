// Copyright 2023 Fred Emmott <fred@fredemmott.com>
// SPDX-License-Identifier: ISC

#include <fmt/core.h>

#include <memory>

#include "Linter.hpp"

namespace FredEmmott::OpenXRLayers {

// Detect API layers with missing files or invalid JSON
class BadInstallationLinter final : public Linter {
  virtual std::vector<std::shared_ptr<LintError>> Lint(
    const std::vector<std::tuple<APILayer, APILayerDetails>>& layers) {
    std::vector<std::shared_ptr<LintError>> errors;
    for (const auto& [layer, details]: layers) {
      if (!std::filesystem::exists(layer.mJSONPath)) {
        errors.push_back(std::make_shared<InvalidLayerLintError>(
          fmt::format("JSON file {} does not exist", layer.mJSONPath.string()),
          layer.mJSONPath));
        continue;
      }

      if (details.mState != APILayerDetails::State::Loaded) {
        errors.push_back(std::make_shared<InvalidLayerLintError>(
          fmt::format(
            "Unable to load details from the JSON file {}",
            layer.mJSONPath.string()),
          layer.mJSONPath));
        continue;
      }

      if (details.mLibraryPath.empty()) {
        errors.push_back(std::make_shared<InvalidLayerLintError>(
          fmt::format(
            "Layer does not specify an implementation in {}",
            layer.mJSONPath.string()),
          layer.mJSONPath));
        continue;
      }

      if (!std::filesystem::exists(details.mLibraryPath)) {
        errors.push_back(std::make_shared<InvalidLayerLintError>(
          fmt::format(
            "Implementation file '{}' does not exist",
            details.mLibraryPath.string()),
          layer.mJSONPath));
        continue;
      }
    }

    return errors;
  }
};

static BadInstallationLinter gInstance;

}// namespace FredEmmott::OpenXRLayers