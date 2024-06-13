// Copyright 2023 Fred Emmott <fred@fredemmott.com>
// SPDX-License-Identifier: ISC

#include <fmt/core.h>

#include <ShlObj.h>

#include "APILayerStore_windows.hpp"
#include "Linter.hpp"

namespace FredEmmott::OpenXRLayers {

// Warn about installations outside of program files
class ProgramFilesLinter final : public Linter {
  virtual std::vector<std::shared_ptr<LintError>> Lint(
    const APILayerStore* store,
    const std::vector<std::tuple<APILayer, APILayerDetails>>& layers) {
    wchar_t* programFilesPath {nullptr};

    auto winStore = dynamic_cast<const WindowsAPILayerStore*>(store);
    if (!winStore) {
#ifndef NDEBUG
      __debugbreak();
#endif
      return {};
    }

    if (
      SHGetKnownFolderPath(
        (winStore->GetRegistryBitness()
         == WindowsAPILayerStore::RegistryBitness::Wow64_64)
          ? FOLDERID_ProgramFilesX64
          : FOLDERID_ProgramFilesX86,
        0,
        NULL,
        &programFilesPath)
      != S_OK) {
      return {};
    }
    const std::filesystem::path programFiles {programFilesPath};

    std::vector<std::shared_ptr<LintError>> errors;
    for (const auto& [layer, details]: layers) {
      if (!layer.IsEnabled()) {
        continue;
      }

      if (details.mLibraryPath.empty()) {
        continue;
      }

      bool isProgramFiles = false;
      auto path = details.mLibraryPath;
      while (path.has_parent_path()) {
        const auto parent = path.parent_path();
        if (parent == path) {
          break;
        }
        path = parent;

        if (path == programFiles) {
          isProgramFiles = true;
          break;
        }
      }

      if (isProgramFiles) {
        continue;
      }

      errors.push_back(std::make_shared<LintError>(
        fmt::format(
          "{} is outside of Program Files; this can cause issue with sandboxed "
          "MS Store games or apps, such as OpenXR Tools for Windows Mixed "
          "Reality.",
          details.mLibraryPath.string()),
        PathSet {layer.mJSONPath}));
    }
    return errors;
  }
};

static ProgramFilesLinter gInstance;

}// namespace FredEmmott::OpenXRLayers