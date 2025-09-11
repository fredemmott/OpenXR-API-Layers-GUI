// Copyright 2023 Fred Emmott <fred@fredemmott.com>
// SPDX-License-Identifier: ISC

#include <fmt/format.h>

#include "Linter.hpp"

namespace FredEmmott::OpenXRLayers {
class DisabledByEnvironmentLinter final : public Linter {
  virtual std::vector<std::shared_ptr<LintError>> Lint(
    const APILayerStore*,
    const std::vector<std::tuple<APILayer, APILayerDetails>>& layers) {
    std::vector<std::shared_ptr<LintError>> errors;

    for (const auto& [layer, details]: layers) {
      if (details.mState != APILayerDetails::State::Loaded) {
        continue;
      }

      const auto& enableEnv = details.mEnableEnvironment;
      if ((!enableEnv.empty()) && !std::getenv(enableEnv.c_str())) {
        errors.push_back(
          std::make_shared<LintError>(
            fmt::format(
              "Layer is disabled, because required environment variable `{}` "
              "is not set",
              enableEnv),
            PathSet {layer.mJSONPath}));
      }

      const auto& disableEnv = details.mDisableEnvironment;
      if (disableEnv.empty()) {
        errors.push_back(
          std::make_shared<LintError>(
            fmt::format(
              "Layer `{}` does not define a `disable_environment` key",
              layer.mJSONPath.string()),
            PathSet {layer.mJSONPath}));
        continue;
      }

      // Disabled if env var is set, even if empty or 0 or 'false'
      if (std::getenv(disableEnv.c_str())) {
        errors.push_back(
          std::make_shared<LintError>(
            fmt::format(
              "Layer is disabled by environment variable `{}`", disableEnv),
            PathSet {layer.mJSONPath}));
      }
    }
    return errors;
  }
};

// Unused, but we care about the constructor
[[maybe_unused]] static DisabledByEnvironmentLinter gInstance;

}// namespace FredEmmott::OpenXRLayers
