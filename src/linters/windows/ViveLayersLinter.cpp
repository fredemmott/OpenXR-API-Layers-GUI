// Copyright 2026 Fred Emmott <fred@fredemmott.com>
// SPDX-License-Identifier: MIT

#include <fmt/core.h>

#include <unordered_set>

#include "APILayerStore.hpp"
#include "Linter.hpp"

// Warn about a registry value that is not a DWORD
namespace FredEmmott::OpenXRLayers {

class ViveLayersLinter final : public Linter {
  std::vector<std::shared_ptr<LintError>> Lint(
    const APILayerStore* store,
    const std::vector<std::tuple<APILayer, APILayerDetails>>& layers) override {
    const auto arch = store->GetArchitectures().get_only();
    if (arch == Architecture::Invalid) {
      return {};
    }
    const auto runtime = Platform::Get().GetActiveRuntime(arch);
    if (!runtime) {
      return {};
    }
    const auto runtimeManifest = runtime->mManifestData;
    if (!runtimeManifest) {
      return {};
    }
    if (runtimeManifest->mName == "SteamVR") {
      return {};
    }
    if (runtimeManifest->mName == "VIVE_OpenXR") {
      // Included with ViveConsole from Steam, but not registered by default.
      // Just used for the Vive Focus and Cosmos
      return {};
    }

    static const std::unordered_set<std::string_view> LayerNames {
      "XR_APILAYER_VIVE_MR",
      "XR_APILAYER_VIVE_hand_tracking",
      "XR_APILAYER_VIVE_facial_tracking",
      "XR_APILAYER_VIVE_srworks",
      "XR_APILAYER_VIVE_xr_tracker",
    };

    std::vector<std::shared_ptr<LintError>> ret;
    for (const auto& [layer, details]: layers) {
      if (!layer.IsEnabled()) {
        continue;
      }
      if (!LayerNames.contains(details.mName)) {
        continue;
      }
      ret.push_back(
        std::make_shared<InvalidLayerStateLintError>(
          fmt::format(
            "{} requires the SteamVR or HTC enterprise runtime, but you are "
            "currently using '{}'; this can cause game crashes or other "
            "issues.",
            details.mName,
            runtimeManifest->mName),
          layer));
    }
    return ret;
  }
};

static ViveLayersLinter gInstance;

}// namespace FredEmmott::OpenXRLayers
