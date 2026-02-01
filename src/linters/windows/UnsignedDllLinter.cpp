// Copyright 2023 Fred Emmott <fred@fredemmott.com>
// SPDX-License-Identifier: ISC

#include <fmt/core.h>

#include <bit>

#include "Linter.hpp"

namespace FredEmmott::OpenXRLayers {

// Warn about DLLs without valid authenticode signatures
class UnsignedDllLinter final : public Linter {
  virtual std::vector<std::shared_ptr<LintError>> Lint(
    const APILayerStore*,
    const std::vector<std::tuple<APILayer, APILayerDetails>>& layers) {
    std::vector<std::shared_ptr<LintError>> errors;
    for (const auto& [layer, details]: layers) {
      if (!layer.IsEnabled()) {
        continue;
      }
      const auto dllPath = details.mLibraryPath.native();
      if (!std::filesystem::exists(dllPath)) {
        continue;
      }

      if (details.mSignature) {
        continue;
      }

      using enum APILayerSignature::Error;
      switch (details.mSignature.error()) {
        case NotSupported:
          return {};
        case FilesystemError:
          continue;
        case Unsigned:
          errors.push_back(
            std::make_shared<LintError>(
              fmt::format(
                "{} does not have a trusted signature; this is very likely to "
                "cause issues with games that use anti-cheat software.",
                details.mLibraryPath.string()),
              LayerKeySet {layer}));
          continue;
        case UntrustedSignature:
          errors.push_back(
            std::make_shared<LintError>(
              fmt::format(
                "Unable to verify a signature for {}; this is "
                "very likely to cause issues with games that use anti-cheat "
                "software.",
                details.mLibraryPath.string()),
              LayerKeySet {layer}));
          continue;
        case Expired:
          // Not seen reports of this so far; don't know if anti-cheats are
          // generally OK with this, or if they recognize the most popular
          // layers now
          errors.push_back(
            std::make_shared<LintError>(
              fmt::format(
                "{} has a signature without a timestamp, from an expired "
                "certificate; This may cause issues with games that use "
                "anti-cheat software.",
                details.mLibraryPath.string()),
              LayerKeySet {layer}));
          continue;
      }
    }
    return errors;
  }
};

static UnsignedDllLinter gInstance;

}// namespace FredEmmott::OpenXRLayers