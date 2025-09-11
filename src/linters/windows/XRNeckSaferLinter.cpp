// Copyright 2023 Fred Emmott <fred@fredemmott.com>
// SPDX-License-Identifier: ISC

#include <fmt/core.h>

#include <ShlObj.h>

#include "Linter.hpp"
#include "windows/WindowsAPILayerStore.hpp"

namespace FredEmmott::OpenXRLayers {

class XRNeckSaferLinter final : public Linter {
  virtual std::vector<std::shared_ptr<LintError>> Lint(
    const APILayerStore* store,
    const std::vector<std::tuple<APILayer, APILayerDetails>>& layers) {
    auto winStore = dynamic_cast<const WindowsAPILayerStore*>(store);
    if (
      winStore->GetRegistryBitness()
      != WindowsAPILayerStore::RegistryBitness::Wow64_64) {
      return {};
    }

    std::vector<std::shared_ptr<LintError>> errors;
    for (const auto& [layer, details]: layers) {
      if (!layer.IsEnabled()) {
        continue;
      }
      if (details.mName != "XR_APILAYER_NOVENDOR_XRNeckSafer") {
        continue;
      }
      if (details.mImplementationVersion != "1") {
        continue;
      }
      errors.push_back(
        std::make_shared<KnownBadLayerLintError>(
          "XRNeckSafer has bugs that can cause issues include game crashes, and "
          "crashes in other API layers. Disable or uninstall it if you have any "
          "issues.",
          layer.mManifestPath));
    }
    return errors;
  }
};

static XRNeckSaferLinter gInstance;

}// namespace FredEmmott::OpenXRLayers