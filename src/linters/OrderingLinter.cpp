
// Copyright 2023 Fred Emmott <fred@fredemmott.com>
// SPDX-License-Identifier: ISC

#include <unordered_map>

#include "KnownLayers.hpp"
#include "Linter.hpp"

namespace FredEmmott::OpenXRLayers {

// Detect dependencies
class OrderingLinter final : public Linter {
 private:
  static std::set<std::string> GetProvides(
    const APILayer& layer,
    const APILayerDetails& details) {
    std::set<std::string> ret {details.mName};
    for (const auto& ext: details.mExtensions) {
      ret.emplace(ext.mName);
    }
    return ret;
  }

 public:
  virtual std::vector<std::shared_ptr<LintError>> Lint(
    const std::vector<std::tuple<APILayer, APILayerDetails>>& layers) {
    std::vector<std::shared_ptr<LintError>> errors;
    std::unordered_map<
      std::string,
      std::vector<std::tuple<APILayer, APILayerDetails>>>
      providers;
    const auto knownLayers = GetKnownLayers();

    for (const auto& [layer, details]: layers) {
      if (knownLayers.contains(details.mName)) {
        const auto consumes = knownLayers.at(details.mName).mConsumes;
        for (const auto& feature: consumes) {
          if (!providers.contains(feature)) {
            continue;
          }

          std::set<std::filesystem::path> affected {layer.mJSONPath};
          for (const auto& [provider, _]: providers.at(feature)) {
            affected.emplace(provider.mJSONPath);
          }

          auto [_, provider] = providers.at(feature).front();
          errors.push_back(std::make_shared<LintError>(
            std::format(
              "As {} consumes {}, it should be above {}.",
              details.mName,
              feature,
              provider.mName),
            affected));
        }
      }

      for (const auto& feature: GetProvides(layer, details)) {
        if (providers.contains(feature)) {
          providers[feature].push_back({layer, details});
        } else {
          providers[feature] = {{layer, details}};
        }
      }
    }

    return errors;
  }
};

static OrderingLinter gInstance;

}// namespace FredEmmott::OpenXRLayers