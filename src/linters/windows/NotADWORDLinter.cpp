// Copyright 2023 Fred Emmott <fred@fredemmott.com>
// SPDX-License-Identifier: ISC

#include <fmt/core.h>

#include "Linter.hpp"

// Warn about a registry value that is not a DWORD
namespace FredEmmott::OpenXRLayers {

class NotADWORDLinter final : public Linter {
  virtual std::vector<std::shared_ptr<LintError>> Lint(
    const APILayerStore*,
    const std::vector<std::tuple<APILayer, APILayerDetails>>& layers) {
    std::vector<std::shared_ptr<LintError>> ret;
    for (const auto& [layer, details]: layers) {
      if (layer.mValue != APILayer::Value::Win32_NotDWORD) {
        continue;
      }
      ret.push_back(
        std::make_shared<InvalidLayerStateLintError>(
          fmt::format(
            "OpenXR requires that layer registry values are DWORDs; `{}` has a "
            "different type. This can cause various issues with other layers "
            "or games.",
            layer.mManifestPath.string()),
          layer));
    }
    return ret;
  }
};

static NotADWORDLinter gInstance;

}// namespace FredEmmott::OpenXRLayers