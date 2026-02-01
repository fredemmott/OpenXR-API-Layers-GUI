// Copyright 2026 Fred Emmott <fred@fredemmott.com>
// SPDX-License-Identifier: MIT

#include <fmt/format.h>
#include <magic_enum/magic_enum.hpp>

#include <ranges>

#include "APILayerStore.hpp"
#include "Linter.hpp"

namespace FredEmmott::OpenXRLayers {

static std::string to_string(const Architectures architectures) {
  const auto bits = architectures.underlying();
  std::vector<std::string_view> parts;
  for (auto&& [value, name]: magic_enum::enum_entries<Architecture>()) {
    if (value == Architecture::Invalid) {
      continue;
    }
    const auto bit = std::to_underlying(value);
    if ((bits & bit) == bit) {
      parts.emplace_back(name);
    }
  }
  switch (parts.size()) {
    case 0:
      return "[none]";
    case 1:
      return std::string {parts.front()};
    case 2:
      return std::format("{} and {}", parts.front(), parts.back());
    default:
      return std::format(
        "{:s}, and {}",
        std::views::join_with(
          std::views::take(parts, parts.size() - 1), std::string_view(", ")),
        parts.back());
  }
}

class ExplicitLayerArchitecturesLinter final : public Linter {
  std::vector<std::shared_ptr<LintError>> Lint(
    const APILayerStore* const store,
    const std::vector<std::tuple<APILayer, APILayerDetails>>& layers) override {
    if (store->GetKind() != APILayer::Kind::Explicit) {
      return {};
    }
    std::vector<std::shared_ptr<LintError>> errors;
    const auto storeArchitectures = store->GetArchitectures();
    for (auto&& layer: std::views::elements<0>(layers)) {
      if (layer.mManifestPath.empty()) {
        continue;
      }
      if (!layer.IsEnabled()) {
        continue;
      }
      const auto architectures = layer.mArchitectures.underlying();
      const auto missing = storeArchitectures.underlying() & ~architectures;
      if (missing == 0) {
        continue;
      }

      errors.emplace_back(new LintError(
        fmt::format(
          "Layer `{}` is enabled via the XR_ENABLE_API_LAYERS environment "
          "variable, but is only available on {}; {} applications may have "
          "errors or crash.",
          layer.GetKey().mValue,
          to_string(layer.mArchitectures),
          to_string(Architectures {static_cast<Architecture>(missing)})),
        {layer}));
    }
    return errors;
  }
};

static ExplicitLayerArchitecturesLinter gInstance;

}// namespace FredEmmott::OpenXRLayers