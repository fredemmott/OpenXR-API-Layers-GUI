// Copyright 2023 Fred Emmott <fred@fredemmott.com>
// SPDX-License-Identifier: ISC

#include <fmt/format.h>

#include "APILayerStore.hpp"
#include "Linter.hpp"
#include "LoaderData.hpp"
#include "Platform.hpp"

namespace FredEmmott::OpenXRLayers {
class SkippedByLoaderLinter final : public Linter {
  virtual std::vector<std::shared_ptr<LintError>> Lint(
    const APILayerStore*,
    const std::vector<std::tuple<APILayer, APILayerDetails>>& layers) {
    const auto loaderData = Platform::Get().GetLoaderData();
    if (!loaderData) {
      return {};
    }

    const auto runtime = Platform::Get().GetActiveRuntime();
    const auto runtimeString
      = runtime ? runtime->mName.value_or(runtime->mPath.string()) : "NONE";

    std::vector<std::shared_ptr<LintError>> errors;

    for (const auto& [layer, details]: layers) {
      if (details.mState != APILayerDetails::State::Loaded) {
        continue;
      }
      if (!layer.IsEnabled()) {
        continue;
      }

      if (!layer.mSource->IsForCurrentArchitecture()) {
        continue;
      }

      if (loaderData->mEnabledLayerNames.contains(details.mName)) {
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
          errors.push_back(
            std::make_shared<LintError>(
              fmt::format(
                "This layer is blocked by your current OpenXR runtime ('{}')",
                runtimeString),
              PathSet {layer.mManifestPath}));
          continue;
        }
      }

      errors.push_back(
        std::make_shared<LintError>(
          fmt::format(
            "Layer appears enabled, but is not loaded by OpenXR; it may be "
            "blocked by your OpenXR runtime ('{}')",
            runtimeString),
          PathSet {layer.mManifestPath}));
    }
    return errors;
  }
};

// Unused, but we care about the constructor
[[maybe_unused]] static SkippedByLoaderLinter gInstance;

}// namespace FredEmmott::OpenXRLayers
