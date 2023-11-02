// Copyright 2023 Fred Emmott <fred@fredemmott.com>
// SPDX-License-Identifier: ISC

#include <ShlObj.h>

#include "Config_windows.hpp"
#include "Linter.hpp"

namespace FredEmmott::OpenXRLayers {

// Warn about legacy versions which may indicate corrupted or outdaed
// installations.
//
// These old versions used MSIX, which can lead to ACL issues. While
// installing a new version will automatically clean these up,
// it's then still possible to co-install an old msix afterwards.
class OutdatedOpenKneeboardLinter final : public Linter {
  virtual std::vector<std::shared_ptr<LintError>> Lint(
    const std::vector<std::tuple<APILayer, APILayerDetails>>& layers) {
    std::vector<std::shared_ptr<LintError>> errors;
    for (const auto& [layer, details]: layers) {
      bool outdated = false;
      if (details.mName == "XR_APILAYER_NOVENDOR_OpenKneeboard") {
        outdated = true;
      } else if (details.mName != "XR_APILAYER_FREDEMMOTT_OpenKneeboard") {
        continue;
      }

      if (Config::APILAYER_HKEY_ROOT == HKEY_CURRENT_USER) {
        outdated = true;
      }

      if (!outdated) {
        const std::string path = details.mLibraryPath.string();
        if (path.find("ProgramData") != std::string::npos) {
          outdated = true;
        } else if (path.find("WindowsApps") != std::string::npos) {
          outdated = true;
        }
      }

      if (!outdated) {
        continue;
      }

      errors.push_back(std::make_shared<InvalidLayerLintError>(
        fmt::format(
          "{} is from an extremely outdated version of OpenKneeboard, which "
          "may cause issues. Remove this API layer, install updates, and "
          "remove any left over old versions from 'Add or Remove Programs'.",
          layer.mJSONPath.string()),
        layer.mJSONPath));
    }
    return errors;
  }
};

static OutdatedOpenKneeboardLinter gInstance;

}// namespace FredEmmott::OpenXRLayers