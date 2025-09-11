// Copyright 2023 Fred Emmott <fred@fredemmott.com>
// SPDX-License-Identifier: ISC

#include <fmt/core.h>

#include <algorithm>
#include <cassert>
#include <ranges>
#include <unordered_map>
#include <unordered_set>

#include "LayerRules.hpp"
#include "Linter.hpp"

namespace FredEmmott::OpenXRLayers {

using LayerExtensions = std::unordered_map<
  LayerID,
  std::unordered_set<ExtensionID, Facet::Hash>,
  Facet::Hash>;

static FacetMap ExpandFacets(
  const FacetMap& facets,
  const LayerExtensions& layers,
  const std::vector<LayerRules>& rules) {
  FacetMap next;
  for (auto&& [facet, trace]: facets) {
    switch (facet.GetKind()) {
      case Facet::Kind::Layer:
        next.emplace(facet, trace);
        break;
      case Facet::Kind::Extension:
        for (auto&& [layer, extensions]: layers) {
          if (std::ranges::contains(extensions, ExtensionID {facet})) {
            auto nextTrace = trace;
            nextTrace.push_front({layer, facet});
            next.emplace(layer, nextTrace);
          }
        }
        break;
      case Facet::Kind::Explicit:
        for (auto&& rule: rules) {
          if (std::ranges::contains(rule.mFacets | std::views::keys, facet)) {
            auto nextTrace = trace;
            nextTrace.push_front({rule.mID, facet});
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

static FacetMap ExpandFacets(
  const LayerRules& rule,
  auto proj,
  const LayerExtensions& layers,
  const std::vector<LayerRules>& rules) {
  FacetMap toExpand = std::invoke(proj, rule);

  for (auto&& mixin: rule.mFacets | std::views::keys) {
    auto mixinIt = std::ranges::find(rules, mixin, &LayerRules::mID);
    if (mixinIt == rules.end()) {
      continue;
    }
    const FacetMap& mixinValues = std::invoke(proj, *mixinIt);
    if (mixinValues.empty()) {
      continue;
    }

    for (auto& value: mixinValues | std::views::keys) {
      toExpand.emplace(
        value,
        FacetTrace {
          {rule.mID, mixin},
        });
    }
  }

  return ExpandFacets(toExpand, layers, rules);
}

/** Replace Extension and Explicit facets with the Layers.
 *
 * The original Facets are retained in the trace.
 */
static std::vector<LayerRules> ExpandRules(
  const std::vector<LayerRules>& rules,
  const std::vector<std::tuple<APILayer, APILayerDetails>>& layers) {
  LayerExtensions layerExtensions;
  for (auto&& [_, details]: layers) {
    layerExtensions.emplace(
      LayerID {details.mName},
      details.mExtensions | std::views::transform([](auto& ext) {
        return ExtensionID {ext.mName};
      }) | std::ranges::to<std::unordered_set<ExtensionID, Facet::Hash>>());
  }

  auto ret = rules | std::views::filter([](auto& it) {
               return it.mID.GetKind() == Facet::Kind::Layer;
             })
    | std::ranges::to<std::vector>();
  auto expandFacets = [&](LayerRules& it, auto proj) {
    FacetMap& facets = std::invoke(proj, it);
    facets = ExpandFacets(it, proj, layerExtensions, rules);
  };
  for (auto& it: ret) {
    expandFacets(it, &LayerRules::mAbove);
    expandFacets(it, &LayerRules::mBelow);
    expandFacets(it, &LayerRules::mConflicts);
    expandFacets(it, &LayerRules::mConflictsPerApp);
  }
  return ret;
}

static std::string ExplainTrace(const FacetTrace& trace) {
  if (trace.empty()) {
    return {};
  }

  if (trace.size() == 1) {
    return std::format("because it {}", trace.front().mWhy.GetDescription());
  } else {
    std::string traceStr;
    const auto reverseTrace = trace | std::views::reverse;
    for (auto it = reverseTrace.begin(); it != reverseTrace.end(); ++it) {
      if (it != reverseTrace.begin()) {
        traceStr
          += (std::ranges::next(it) == reverseTrace.end()) ? ", and " : ", ";
      }

      const auto& [what, why] = *it;
      traceStr
        += std::format("{} {}", what.GetDescription(), why.GetDescription());
    }
    return std::format("because {}", traceStr);
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
      | std::ranges::to<std::vector>();

    std::vector<std::shared_ptr<LintError>> errors;

    const auto rules = ExpandRules(GetLayerRules(), layers);

    std::unordered_map<LayerID, size_t, Facet::Hash> layerIndices;
    for (auto&& [_, details]: layers) {
      layerIndices.emplace(LayerID {details.mName}, layerIndices.size());
    }

    for (auto&& [layer, details]: layers) {
      const LayerID layerID {details.mName};
      const auto layerIndex = layerIndices.at(layerID);

      const auto rule
        = std::ranges::find(rules, Facet {layerID}, &LayerRules::mID);
      if (rule == rules.end()) {
        continue;
      }
      using Position = OrderingLintError::Position;

      // LINT RULE: Above
      for (auto&& [other, trace]: rule->mAbove) {
        const auto it = layerIndices.find(LayerID {other});
        if (it == layerIndices.end()) {
          continue;
        }

        if (it->second > layerIndex) {
          continue;
        }
        errors.push_back(MakeOrderingLintError(
          {layer, details}, Position::Above, layers.at(it->second), trace));
      }

      // LINT RULE: Below
      for (auto&& [facet, trace]: rule->mBelow) {
        const auto it = layerIndices.find(LayerID {facet});
        if (it == layerIndices.end()) {
          continue;
        }

        if (it->second < layerIndex) {
          continue;
        }

        errors.push_back(MakeOrderingLintError(
          {layer, details}, Position::Below, layers.at(it->second), trace));
      }

      // LINT RULE: Conflicts
      for (const auto& facet: rule->mConflicts | std::views::keys) {
        const auto otherIt
          = std::ranges::find(layers, facet.GetID(), [](const auto& it) {
              return std::get<1>(it).mName;
            });
        if (otherIt == layers.end()) {
          continue;
        }

        const auto& [other, otherDetails] = *otherIt;
        errors.push_back(
          std::make_shared<LintError>(
            fmt::format(
              "{} ({}) and {} ({}) are incompatible; you must remove or "
              "disable one.",
              details.mName,
              layer.mJSONPath.string(),
              otherDetails.mName,
              other.mJSONPath.string()),
            PathSet {layer.mJSONPath, other.mJSONPath}));
      }

      // LINT RULE: ConflictsPerApp
      for (const auto& facet: rule->mConflictsPerApp | std::views::keys) {
        const auto otherIt
          = std::ranges::find(layers, facet.GetID(), [](const auto& it) {
              return std::get<1>(it).mName;
            });
        if (otherIt == layers.end()) {
          continue;
        }

        const auto& [other, otherDetails] = *otherIt;
        errors.push_back(
          std::make_shared<LintError>(
            fmt::format(
              "{} ({}) and {} ({}) are incompatible; make sure that games "
              "using "
              "{} are disabled in {}.",
              details.mName,
              layer.mJSONPath.string(),
              otherDetails.mName,
              other.mJSONPath.string(),
              details.mName,
              otherDetails.mName),
            PathSet {layer.mJSONPath, other.mJSONPath}));
      }
    }

    return errors;
  }
};

// Unused, but we care about the constructor
[[maybe_unused]] static OrderingLinter gInstance;

}// namespace FredEmmott::OpenXRLayers