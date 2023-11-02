
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

    const auto knownLayers = GetKnownLayers();
    if (!knownLayers.contains(details.mName)) {
      return ret;
    }
    const auto& meta = knownLayers.at(details.mName);
    for (const auto& feature: meta.mProvides) {
      ret.emplace(feature);
    }

    return ret;
  }

 public:
  virtual std::vector<std::shared_ptr<LintError>> Lint(
    const std::vector<std::tuple<APILayer, APILayerDetails>>& layers) {
    std::vector<std::shared_ptr<LintError>> errors;

    using ReqMap = std::unordered_map<
      std::string,
      std::vector<std::tuple<APILayer, APILayerDetails>>>;
    ReqMap providers;
    ReqMap consumesFromAbove;

    const auto knownLayers = GetKnownLayers();

    for (const auto& [layer, details]: layers) {
      if (knownLayers.contains(details.mName)) {
        const auto meta = knownLayers.at(details.mName);

        const auto consumes = meta.mAbove;
        for (const auto& feature: consumes) {
          if (!providers.contains(feature)) {
            continue;
          }

          std::set<std::filesystem::path> affected {layer.mJSONPath};
          for (const auto& [provider, _]: providers.at(feature)) {
            affected.emplace(provider.mJSONPath);
          }

          auto [providerBasic, providerDetails] = providers.at(feature).front();
          errors.push_back(std::make_shared<LintError>(
            std::format(
              "As {} ({}) consumes '{}', it should be above {} ({}).",
              details.mName,
              layer.mJSONPath.string(),
              feature,
              providerDetails.mName,
              providerBasic.mJSONPath.string()),
            affected));
        }

        for (const auto& feature: meta.mBelow) {
          if (consumesFromAbove.contains(feature)) {
            consumesFromAbove[feature].push_back({layer, details});
          } else {
            consumesFromAbove[feature] = {{layer, details}};
          }
        }
      }

      for (const auto& feature: GetProvides(layer, details)) {
        if (consumesFromAbove.contains(feature)) {
          PathSet affected {layer.mJSONPath};
          const auto& consumers = consumesFromAbove.at(feature);
          for (const auto& [consumer, _]: consumers) {
            affected.emplace(consumer.mJSONPath);
          }
          const auto& [consumerBasic, consumerDetails] = consumers.back();
          errors.push_back(std::make_shared<LintError>(
            std::format(
              "As {} ({}) consumes '{}', it should be below {} ({}).",
              consumerDetails.mName,
              consumerBasic.mJSONPath.string(),
              feature,
              details.mName,
              layer.mJSONPath.string()),
            affected));
        }

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