// Copyright 2023 Fred Emmott <fred@fredemmott.com>
// SPDX-License-Identifier: ISC

#include <fmt/format.h>

#include "APILayerStore.hpp"
#include "Linter.hpp"
#include "LoaderData.hpp"
#include "Platform.hpp"

namespace FredEmmott::OpenXRLayers {
class SkippedByLoaderLinter final : public Linter {
 public:
  virtual std::vector<std::shared_ptr<LintError>> Lint(
    const APILayerStore* store,
    const std::vector<std::tuple<APILayer, APILayerDetails>>& layers) {
    std::vector<std::shared_ptr<LintError>> errors;

    for (const auto arch: store->GetArchitectures().enumerate()) {
      Lint(std::back_inserter(errors), arch, layers);
    }
    return errors;
  }

 private:
  static void Lint(
    std::back_insert_iterator<std::vector<std::shared_ptr<LintError>>> out,
    const Architecture arch,
    const std::vector<std::tuple<APILayer, APILayerDetails>>& layers) {
    const auto loaderData = Platform::Get().GetLoaderData(arch);
    if (!loaderData) {
      return;
    }

    const auto runtime = Platform::Get().GetActiveRuntime();
    const auto runtimeString
      = runtime ? runtime->mName.value_or(runtime->mPath.string()) : "NONE";

    for (const auto& [layer, details]: layers) {
      if (details.mState != APILayerDetails::State::Loaded) {
        continue;
      }
      if (!layer.IsEnabled()) {
        continue;
      }

      if (layer.GetKind() != APILayer::Kind::Implicit) {
        continue;
      }

      if (!layer.mSource->GetArchitectures().contains(arch)) {
        continue;
      }

      if (std::ranges::contains(
            loaderData->mEnabledLayerNames, details.mName)) {
        continue;
      }

      const auto& enableEnv = details.mEnableEnvironment;
      if ((!enableEnv.empty()) && !std::getenv(enableEnv.c_str())) {
        continue;
      }

      const auto& disableEnv = details.mDisableEnvironment;
      if (!disableEnv.empty()) {
        if (std::getenv(disableEnv.c_str())) {
          continue;
        }
        if (
          loaderData->mEnvironmentVariablesAfterLoader.contains(disableEnv)
          && !loaderData->mEnvironmentVariablesBeforeLoader.contains(
            disableEnv)) {
          *out = std::make_shared<LintError>(
            fmt::format(
              "Layer `{}` is blocked by your current OpenXR runtime ('{}')",
              layer.mManifestPath.string(),
              runtimeString),
            LayerKeySet {layer});
          continue;
        }
      }

      *out = std::make_shared<LintError>(
        fmt::format(
          "Layer `{}` appears enabled, but is not loaded by OpenXR; it may "
          "be blocked by your OpenXR runtime ('{}')",
          layer.mManifestPath.string(),
          runtimeString),
        LayerKeySet {layer});
    }
  }
};

// Unused, but we care about the constructor
[[maybe_unused]] static SkippedByLoaderLinter gInstance;

}// namespace FredEmmott::OpenXRLayers
