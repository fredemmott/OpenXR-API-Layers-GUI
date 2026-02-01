// Copyright 2026 Fred Emmott <fred@fredemmott.com>
// SPDX-License-Identifier: MIT

#include <ranges>

#include "Linter.hpp"

namespace FredEmmott::OpenXRLayers {

namespace {
constexpr std::string_view LayerName {"XR_APILAYER_ULTRALEAP_hand_tracking"};

class MustBeLastLayerLintError final : public FixableLintError {
 public:
  MustBeLastLayerLintError(
    const std::string& description,
    const APILayer& layer)
    : FixableLintError(description, {layer}),
      mLayer(layer) {}

  std::vector<APILayer> Fix(const std::vector<APILayer>& allLayers) override {
    auto ret = allLayers;
    const auto it = std::ranges::find(ret, mLayer);
    if (it == ret.end()) {
      return ret;
    }
    std::swap(*it, *std::ranges::prev(ret.end()));
    return ret;
  }

 private:
  APILayer::Key mLayer;
};

}// namespace

class UltraleapLastLinter final : public Linter {
 public:
  std::vector<std::shared_ptr<LintError>> Lint(
    const APILayerStore* store,
    const std::vector<std::tuple<APILayer, APILayerDetails>>& layers) override {
    const auto enabledLayers
      = std::views::filter(
          layers, [](const auto& tuple) { return get<0>(tuple).IsEnabled(); })
      | std::ranges::to<std::vector>();
    const auto it = std::ranges::find_if(enabledLayers, [](const auto& tuple) {
      return get<1>(tuple).mName == LayerName;
    });
    if (it == enabledLayers.end()) {
      return {};
    }
    {
      const auto idx = std::ranges::distance(enabledLayers.begin(), it);
      if (idx == enabledLayers.size() - 1) {
        return {};
      }
    }
    const auto& [layer, details] = *it;
    if (details.mImplementationVersion != "1") {
      return {};
    }

    return {std::make_shared<MustBeLastLayerLintError>(
      "The Ultraleap hand tracking layer has bugs that break other API layers "
      "unless it is the very last API layer",
      layer)};
  }
};

static UltraleapLastLinter gInstance;

}// namespace FredEmmott::OpenXRLayers