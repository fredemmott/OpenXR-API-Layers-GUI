// Copyright 2023 Fred Emmott <fred@fredemmott.com>
// SPDX-License-Identifier: ISC

#include <fmt/core.h>

#include <ShlObj.h>

#include "Linter.hpp"
#include "windows/WindowsAPILayerStore.hpp"

namespace FredEmmott::OpenXRLayers {

class OpenXRToolkitLinter final : public Linter {
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
      if (details.mName != "XR_APILAYER_MBUCCHIA_toolkit") {
        continue;
      }
      errors.push_back(std::make_shared<KnownBadLayerLintError>(
        "OpenXR Toolkit is unsupported, and is known to cause crashes and "
        "other issues in modern games; you should disable it if you encounter "
        "problems.",
        layer.mManifestPath));
    }
    return errors;
  }
};

static OpenXRToolkitLinter gInstance;

}// namespace FredEmmott::OpenXRLayers