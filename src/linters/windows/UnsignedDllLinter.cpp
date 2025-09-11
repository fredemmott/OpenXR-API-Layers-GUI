// Copyright 2023 Fred Emmott <fred@fredemmott.com>
// SPDX-License-Identifier: ISC

#include <fmt/core.h>

#include <bit>

#include "Linter.hpp"

// clang-format off
#include <Windows.h>
#include <WinTrust.h>
#include <SoftPub.h>
// clang-format on

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

      WINTRUST_FILE_INFO fileInfo {
        .cbStruct = sizeof(WINTRUST_FILE_INFO),
        .pcwszFilePath = dllPath.c_str(),
      };
      WINTRUST_DATA wintrustData {
        .cbStruct = sizeof(WINTRUST_DATA),
        .dwUIChoice = WTD_UI_NONE,
        .fdwRevocationChecks = WTD_REVOCATION_CHECK_NONE,
        .dwUnionChoice = WTD_CHOICE_FILE,
        .pFile = &fileInfo,
        .dwStateAction = WTD_STATEACTION_VERIFY,
      };

      GUID policyGuid = WINTRUST_ACTION_GENERIC_VERIFY_V2;
      const auto result = WinVerifyTrust(
        static_cast<HWND>(INVALID_HANDLE_VALUE), &policyGuid, &wintrustData);
      switch (result) {
        case 0:
          continue;
        case TRUST_E_SUBJECT_NOT_TRUSTED:
        case TRUST_E_NOSIGNATURE:
          errors.push_back(std::make_shared<LintError>(
            fmt::format(
              "{} does not have a trusted signature; this is very likely to "
              "cause issues with games that use anti-cheat software.",
              details.mLibraryPath.string()),
            PathSet {layer.mManifestPath}));
          break;
        case CERT_E_EXPIRED:
          // Not seen reports of this so far; don't know if anti-cheats are
          // generally OK with this, or if they recognize the most popular
          // layers now
          errors.push_back(std::make_shared<LintError>(
            fmt::format(
              "{} has a signature without a timestamp, from an expired "
              "certificate; This may cause issues with games that use "
              "anti-cheat software.",
              details.mLibraryPath.string()),
            PathSet {layer.mManifestPath}));
          break;
        default:
          errors.push_back(std::make_shared<LintError>(
            fmt::format(
              "Unable to verify a signature for {} - error {:#0x}; this is "
              "very likely to cause issues with games that use anti-cheat "
              "software.",
              details.mLibraryPath.string(),
              std::bit_cast<uint32_t>(result)),
            PathSet {layer.mManifestPath}));
          break;
      }
    }
    return errors;
  }
};

static UnsignedDllLinter gInstance;

}// namespace FredEmmott::OpenXRLayers