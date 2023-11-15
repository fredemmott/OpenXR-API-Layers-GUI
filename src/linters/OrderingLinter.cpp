
// Copyright 2023 Fred Emmott <fred@fredemmott.com>
// SPDX-License-Identifier: ISC

#include <fmt/core.h>

#include <cassert>
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
    ReqMap consumesFromBelow;
    ReqMap consumesFromAbove;
    ReqMap conflicts;
    ReqMap conflictsPerApp;

    const auto knownLayers = GetKnownLayers();

    for (const auto& [layer, details]: layers) {
      if (!knownLayers.contains(details.mName)) {
        continue;
      }

      const auto populate = [&layer, &details](auto& out, const auto& in) {
        for (const auto& feature: in) {
          if (out.contains(feature)) {
            out[feature].push_back({layer, details});
          } else {
            out[feature] = {{layer, details}};
          }
        }
      };

      const auto meta = knownLayers.at(details.mName);
      populate(consumesFromAbove, meta.mAbove);
      populate(consumesFromBelow, meta.mBelow);
      populate(conflicts, meta.mConflicts);
      populate(conflictsPerApp, meta.mConflictsPerApp);
    }

    for (auto providerIt = layers.begin(); providerIt != layers.end();
         providerIt++) {
      auto [provider, providerDetails] = *providerIt;
      const auto provides = this->GetProvides(provider, providerDetails);

      auto targetIt = providerIt;
      std::optional<
        std::tuple<OrderingLintError::Position, APILayer, APILayerDetails>>
        move;
      std::string conflictFeature;
      std::unordered_map<std::string, PathSet> affectedLayersByFeature;

      using Position = OrderingLintError::Position;

      for (const auto& feature: provides) {
        // LINT RULE: Hard conflicts
        if (conflicts.contains(feature)) {
          for (const auto& [other, otherDetails]: conflicts.at(feature)) {
            if (!(provider.mIsEnabled && other.mIsEnabled)) {
              continue;
            }
            errors.push_back(std::make_shared<LintError>(
              fmt::format(
                "{} ({}) and {} ({}) are incompatible; you must remove or "
                "disable one.",
                providerDetails.mName,
                provider.mJSONPath.string(),
                otherDetails.mName,
                other.mJSONPath.string()),
              PathSet {provider.mJSONPath, other.mJSONPath}));
          }
        }

        if (conflictsPerApp.contains(feature)) {
          for (const auto& [other, otherDetails]: conflictsPerApp.at(feature)) {
            if (!(provider.mIsEnabled && other.mIsEnabled)) {
              continue;
            }
            errors.push_back(std::make_shared<LintError>(
              fmt::format(
                "Make sure that games using {} ({}) are disabled in {} ({})",
                providerDetails.mName,
                provider.mJSONPath.string(),
                otherDetails.mName,
                other.mJSONPath.string()),
              PathSet {provider.mJSONPath, other.mJSONPath}));
          }
        }

        // LINT RULE: Consumes from above
        if (consumesFromAbove.contains(feature)) {
          const auto& consumers = consumesFromAbove.at(feature);
          for (const auto& consumerPair: consumers) {
            auto consumerIt = std::ranges::find(layers, consumerPair);
            if (consumerIt == layers.end()) {
              continue;
            }

            if (consumerIt <= providerIt) {
              continue;
            }
            const auto& [consumer, consumerDetails] = consumerPair;
            if (affectedLayersByFeature.contains(feature)) {
              affectedLayersByFeature.at(feature).emplace(consumer.mJSONPath);
            } else {
              affectedLayersByFeature[feature] = {consumer.mJSONPath};
            }

            if (consumerIt <= targetIt) {
              continue;
            }
            targetIt = consumerIt;
            conflictFeature = feature;

            move = {Position::Below, consumer, consumerDetails};
          }
        }

        // LINT RULE: consumes from below
        if (consumesFromBelow.contains(feature)) {
          const auto& consumers = consumesFromBelow.at(feature);
          for (const auto& consumerPair: consumers) {
            auto consumerIt = std::ranges::find(layers, consumerPair);
            if (consumerIt == layers.end()) {
              continue;
            }

            if (consumerIt >= providerIt) {
              continue;
            }

            const auto& [consumer, consumerDetails] = consumerPair;
            if (affectedLayersByFeature.contains(feature)) {
              affectedLayersByFeature.at(feature).emplace(consumer.mJSONPath);
            } else {
              affectedLayersByFeature[feature] = {consumer.mJSONPath};
            }

            if (consumerIt >= targetIt) {
              continue;
            }
            targetIt = consumerIt;
            conflictFeature = feature;

            move = {Position::Above, consumer, consumerDetails};
          }
        }
      }

      if (targetIt == providerIt) {
        continue;
      }

      assert(move);
      assert(!conflictFeature.empty());
      assert(affectedLayersByFeature.contains(conflictFeature));

      auto affected = affectedLayersByFeature.at(conflictFeature);
      affected.emplace(provider.mJSONPath);

      const auto& [position, relativeTo, relativeToDetails] = *move;

      errors.push_back(std::make_shared<OrderingLintError>(
        fmt::format(
          "Because {} ({}) provides {}, it must be {} {} ({})",
          providerDetails.mName,
          provider.mJSONPath.string(),
          conflictFeature,
          (position == Position::Above ? "above" : "below"),
          relativeToDetails.mName,
          relativeTo.mJSONPath.string()),
        provider.mJSONPath,
        position,
        relativeTo.mJSONPath));
    }

    return errors;
  }
};

static OrderingLinter gInstance;

}// namespace FredEmmott::OpenXRLayers