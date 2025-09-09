// Copyright 2023 Fred Emmott <fred@fredemmott.com>
// SPDX-License-Identifier: ISC

#include "Linter.hpp"

#include <fmt/format.h>

namespace FredEmmott::OpenXRLayers {
class DisabledByEnvironmentLinter final : public Linter {
  virtual std::vector<std::shared_ptr<LintError>> Lint(
    const APILayerStore*,
    const std::vector<std::tuple<APILayer, APILayerDetails>>& layers) {
    std::vector<std::shared_ptr<LintError>> errors;

    for (const auto& [layer, details]: layers) {
      if (!std::filesystem::exists(layer.mJSONPath)) {
        continue;
      }

      const auto& envVar = details.mDisableEnvironment;
      if (envVar.empty()) {
        errors.push_back(std::make_shared<LintError>(
          fmt::format("Layer `{}` does not define a `disable_environment` key",
            layer.mJSONPath.string()), PathSet {layer.mJSONPath}));
        continue;
      }

      // Disabled if env var is set, even if empty or 0 or 'false'
      if (std::getenv(envVar.c_str())) {
        errors.push_back(std::make_shared<LintError>(
          fmt::format("Layer `{}` is disabled by environment variable `{}`",
            layer.mJSONPath.string(), envVar), PathSet {layer.mJSONPath}));
      }
    }
    return errors;
  }
};

// Unused, but we care about the constructor
[[maybe_unused]] static DisabledByEnvironmentLinter gInstance;

}
