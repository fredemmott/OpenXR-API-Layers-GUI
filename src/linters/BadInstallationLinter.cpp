// Copyright 2023 Fred Emmott <fred@fredemmott.com>
// SPDX-License-Identifier: ISC

#include <boost/mpl/assert.hpp>
#include <fmt/core.h>

#include <cassert>
#include <memory>

#include "Linter.hpp"

namespace FredEmmott::OpenXRLayers {

// Detect API layers with missing files or invalid JSON
class BadInstallationLinter final : public Linter {
  virtual std::vector<std::shared_ptr<LintError>> Lint(
    const APILayerStore*,
    const std::vector<std::tuple<APILayer, APILayerDetails>>& layers) {
    std::vector<std::shared_ptr<LintError>> errors;
    for (const auto& [layer, details]: layers) {
      if (layer.mValue == APILayer::Value::EnabledButAbsent) {
        assert(layer.GetKind() == APILayer::Kind::Explicit);
        errors.push_back(
          std::make_shared<InvalidLayerLintError>(
            fmt::format(
              "`{}` is in XR_ENABLE_API_LAYERS, but is not installed",
              layer.GetKey().mValue),
            layer));
        continue;
      }
      if (layer.mManifestPath.empty()) {
        errors.push_back(
          std::make_shared<InvalidLayerLintError>(
            fmt::format(
              "Layer `{}` has empty manifest path", layer.GetKey().mValue),
            layer));
        continue;
      }

      if (details.mState != APILayerDetails::State::Loaded) {
        errors.push_back(
          std::make_shared<InvalidLayerLintError>(
            fmt::format(
              "Unable to load details from the manifest file `{}`",
              layer.mManifestPath.string()),
            layer));
        continue;
      }

      if (details.mLibraryPath.empty()) {
        errors.push_back(
          std::make_shared<InvalidLayerLintError>(
            fmt::format(
              "Layer does not specify an implementation in `{}`",
              layer.mManifestPath.string()),
            layer));
        continue;
      }

      if (!std::filesystem::exists(details.mLibraryPath)) {
        errors.push_back(
          std::make_shared<InvalidLayerLintError>(
            fmt::format(
              "Implementation file `{}` does not exist",
              details.mLibraryPath.string()),
            layer));
        continue;
      }
    }

    return errors;
  }
};

static BadInstallationLinter gInstance;

}// namespace FredEmmott::OpenXRLayers