// Copyright 2023 Fred Emmott <fred@fredemmott.com>
// SPDX-License-Identifier: ISC

#include <fmt/core.h>

#include <unordered_map>
#include <vector>

#include "Linter.hpp"

namespace FredEmmott::OpenXRLayers {

// Detect multiple enabled versions of the same layer
class DuplicatesLinter final : public Linter {
 public:
  virtual std::vector<std::shared_ptr<LintError>> Lint(
    const APILayerStore*,
    const std::vector<std::tuple<APILayer, APILayerDetails>>& layers) {
    std::unordered_map<std::string, LayerKeySet> byName;
    for (const auto& [layer, details]: layers) {
      if (!layer.IsEnabled()) {
        continue;
      }

      if (details.mState != APILayerDetails::State::Loaded) {
        continue;
      }

      if (byName.contains(details.mName)) {
        byName.at(details.mName).emplace(layer);
      } else {
        byName[details.mName] = {layer};
      }
    }

    std::vector<std::shared_ptr<LintError>> errors;
    for (const auto& [name, keys]: byName) {
      if (keys.size() == 1) {
        continue;
      }

      auto text = fmt::format("Multiple copies of {} are enabled:", name);
      for (auto&& key: keys) {
        text += fmt::format("\n- {}", key.mValue);
      }

      errors.push_back(std::make_shared<LintError>(text, keys));
    }
    return errors;
  }
};

static DuplicatesLinter gInstance;
}// namespace FredEmmott::OpenXRLayers