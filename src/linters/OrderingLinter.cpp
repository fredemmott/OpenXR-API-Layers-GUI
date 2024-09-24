
// Copyright 2023 Fred Emmott <fred@fredemmott.com>
// SPDX-License-Identifier: ISC

#include <fmt/core.h>

#include <cassert>
#include <ranges>
#include <unordered_map>
#include <unordered_set>

#include "KnownLayers.hpp"
#include "Linter.hpp"

namespace FredEmmott::OpenXRLayers {

// Not accurate, but close enough for basic usage
namespace std23::ranges {
template <template <class...> class C, class... Args>
class to {
 public:
  template <std::ranges::range R>
  constexpr auto operator()(R&& r) const {
    return C<std::ranges::range_value_t<R>, Args...> {
      std::ranges::begin(r),
      std::ranges::end(r),
    };
  }
};
template <std::ranges::range R, template <class...> class C, class... Args>
constexpr auto operator|(R&& r, to<C, Args...> const& t) {
  return t(std::forward<R>(r));
}

template <std::ranges::range R, class Proj = std::identity>
constexpr bool contains(R&& r, const auto& value, Proj proj = Proj {}) {
  return std::ranges::find(r, value, proj) != std::ranges::end(r);
}
}// namespace std23::ranges

using LayerExtensions = std::unordered_map<
  LayerID,
  std::unordered_set<ExtensionID, Facet::Hash>,
  Facet::Hash>;

static FacetMap ExpandFacets(
  const FacetMap& facets,
  const LayerExtensions& layers,
  const std::vector<KnownLayer>& rules) {
  FacetMap next;
  for (auto&& [facet, trace]: facets) {
    FacetTrace nextTrace {facet};
    std::ranges::copy(trace, std::back_inserter(nextTrace));

    switch (facet.GetKind()) {
      case Facet::Kind::Layer:
        next.emplace(facet, trace);
        break;
      case Facet::Kind::Extension:
        for (auto&& [layer, extensions]: layers) {
          if (std23::ranges::contains(extensions, ExtensionID {facet})) {
            next.emplace(layer, nextTrace);
          }
        }
        break;
      case Facet::Kind::Explicit:
        for (auto&& rule: rules) {
          if (std23::ranges::contains(rule.mFacets | std::views::keys, facet)) {
            next.emplace(rule.mID, nextTrace);
          }
        }
        break;
    }
  }

  if (next == facets) {
    for (const auto& it: facets | std::views::keys) {
      assert(it.GetKind() == Facet::Kind::Layer);
    }
    return next;
  }

  return ExpandFacets(next, layers, rules);
}

/** Replace Extension and Explicit facets with the Layers.
 *
 * The original Facets are retained in the trace.
 */
static std::vector<KnownLayer> ExpandRules(
  const std::vector<KnownLayer>& rules,
  const std::vector<std::tuple<APILayer, APILayerDetails>>& layers) {
  LayerExtensions layerExtensions;
  for (auto&& [_, details]: layers) {
    layerExtensions.emplace(
      LayerID {details.mName},
      details.mExtensions | std::views::transform([](auto& ext) {
        return ExtensionID {ext.mName};
      }) | std23::ranges::to<std::unordered_set, Facet::Hash>());
  }

  auto ret = rules | std::views::filter([](auto& it) {
               return it.mID.GetKind() == Facet::Kind::Layer;
             })
    | std23::ranges::to<std::vector>();
  auto expandFacets = [&](auto& facets) {
    facets = ExpandFacets(facets, layerExtensions, rules);
  };
  for (auto& it: ret) {
    expandFacets(it.mFacets);
    expandFacets(it.mAbove);
    expandFacets(it.mBelow);
    expandFacets(it.mConflicts);
    expandFacets(it.mConflictsPerApp);
  }
  return ret;
}

static std::string ExplainTrace(const FacetTrace& trace) {
  if (trace.empty()) {
    return {};
  }

  if (trace.size() == 1) {
    return std::format("because it {}", trace.front().GetDescription());
  } else {
    std::string traceStr;
    for (auto&& it: trace) {
      if (traceStr.empty()) {
        traceStr += " ➡️ ";
      }
      traceStr += it.GetDescription();
    }
    return std::format(
      "because it {} ({})", trace.back().GetDescription(), traceStr);
  }
}

static auto MakeOrderingLintError(
  const std::tuple<APILayer, APILayerDetails>& layerToMove,
  OrderingLintError::Position position,
  const std::tuple<APILayer, APILayerDetails>& relativeTo,
  const FacetTrace& trace) {
  const auto toMoveName = std::get<1>(layerToMove).mName;
  const auto toMovePath = std::get<0>(layerToMove).mJSONPath;
  const auto relativeToName = std::get<1>(relativeTo).mName;
  const auto relativeToPath = std::get<0>(relativeTo).mJSONPath;

  auto msg = std::format(
    "{} ({}) must be {} {} ({})",
    toMoveName,
    toMovePath.string(),
    position == OrderingLintError::Position::Above ? "above" : "below",
    relativeToName,
    relativeToPath.string());
  if (!trace.empty()) {
    msg += std::format(" {}.", ExplainTrace(trace));
  } else {
    msg += ".";
  }

  return std::make_shared<OrderingLintError>(
    msg, toMovePath, position, relativeToPath);
}

// Detect dependencies
class OrderingLinter final : public Linter {
 public:
  std::vector<std::shared_ptr<LintError>> Lint(
    const APILayerStore*,
    const std::vector<std::tuple<APILayer, APILayerDetails>>& allLayers)
    override {
    const auto layers
      = allLayers | std::views::filter([](const auto& it) {
          const auto& [layer, details] = it;
          if (!layer.IsEnabled()) {
            return false;
          }
          if (details.mState != APILayerDetails::State::Loaded) {
            return false;
          }
          return true;
        })
      | std23::ranges::to<std::vector>();

    std::vector<std::shared_ptr<LintError>> errors;

    const auto rules = ExpandRules(
      GetKnownLayers() | std::views::values | std23::ranges::to<std::vector>(),
      layers);

    using ReqMap = std::unordered_map<
      std::string,
      std::vector<std::tuple<APILayer, APILayerDetails>>>;
    ReqMap consumesFromBelow;
    ReqMap consumesFromAbove;
    ReqMap conflicts;
    ReqMap conflictsPerApp;

    const auto knownLayers = GetKnownLayers();

    std::unordered_map<LayerID, size_t, Facet::Hash> layerIndices;
    for (auto&& [_, details]: layers) {
      layerIndices.emplace(LayerID {details.mName}, layerIndices.size());
    }

    for (auto&& [layer, details]: layers) {
      const LayerID layerID {details.mName};
      const auto layerIndex = layerIndices.at(layerID);

      const auto rule
        = std::ranges::find(rules, Facet {layerID}, &KnownLayer::mID);
      if (rule == rules.end()) {
        continue;
      }
      using Position = OrderingLintError::Position;

      for (auto&& [other, trace]: rule->mAbove) {
        const auto it = layerIndices.find(LayerID {other});
        if (it == layerIndices.end()) {
          continue;
        }

        if (it->second < layerIndex) {
          errors.push_back(MakeOrderingLintError(
            {layer, details}, Position::Above, layers.at(it->second), trace));
        }
      }

      for (auto&& [facet, trace]: rule->mBelow) {
        const auto it = layerIndices.find(LayerID {facet});
        if (it == layerIndices.end()) {
          continue;
        }

        if (it->second > layerIndex) {
          errors.push_back(MakeOrderingLintError(
            {layer, details}, Position::Below, layers.at(it->second), trace));
        }
      }

      std::optional<
        std::tuple<OrderingLintError::Position, APILayer, APILayerDetails>>
        move;
      std::string conflictFeature;
      std::unordered_map<std::string, PathSet> affectedLayersByFeature;

#ifdef FIXME_IMPLEMENT_CONFLICTS
      for (const auto& feature: provides) {
        // LINT RULE: Hard conflicts
        if (conflicts.contains(feature)) {
          for (const auto& [other, otherDetails]: conflicts.at(feature)) {
            if (!(provider.IsEnabled() && other.IsEnabled())) {
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
            if (!(provider.IsEnabled() && other.IsEnabled())) {
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
          for (auto&& consumerPair: consumesFromAbove.at(feature)) {
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
          for (auto&& consumerPair: consumesFromBelow.at(feature)) {
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

      const auto errorString = (providerDetails.mName == conflictFeature)
        ? fmt::format(
            "{} ({}) must be {} {} ({})",
            providerDetails.mName,
            provider.mJSONPath.string(),
            (position == Position::Above ? "above" : "below"),
            relativeToDetails.mName,
            relativeTo.mJSONPath.string())
        : fmt::format(
            "Because {} ({}) provides {}, it must be {} {} ({})",
            providerDetails.mName,
            provider.mJSONPath.string(),
            conflictFeature,
            (position == Position::Above ? "above" : "below"),
            relativeToDetails.mName,
            relativeTo.mJSONPath.string());

      errors.push_back(std::make_shared<OrderingLintError>(
        errorString, provider.mJSONPath, position, relativeTo.mJSONPath));
#endif
    }

    return errors;
  }
};

// Unused, but we care about the constructor
[[maybe_unused]] static OrderingLinter gInstance;

}// namespace FredEmmott::OpenXRLayers