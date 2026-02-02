// Copyright 2025 Fred Emmott <fred@fredemmott.com>
// SPDX-License-Identifier: MIT
#pragma once
#include <nlohmann/json_fwd.hpp>
#include <openxr/openxr.h>

#include <filesystem>
#include <string>
#include <system_error>
#include <unordered_set>
#include <variant>

#include "Architectures.hpp"

namespace FredEmmott::OpenXRLayers {

struct LoaderData {
  struct PendingError {};
  struct PipeCreationError {
    std::error_code mError;
  };
  struct PipeAttributeError {
    std::error_code mError;
  };
  struct CanNotFindCurrentExecutableError {
    std::error_code mError;
  };
  struct CanNotSpawnError {
    std::error_code mError;
  };
  struct BadExitCodeError {
    uint32_t mExitCode;
  };
  struct InvalidJSONError {
    std::string mExplanation;
  };
  using Error = std::variant<
    PendingError,
    PipeCreationError,
    PipeAttributeError,
    CanNotFindCurrentExecutableError,
    CanNotSpawnError,
    BadExitCodeError,
    InvalidJSONError>;

  Architecture mArchitecture {GetBuildArchitecture()};
  XrResult mQueryExtensionsResult {XR_RESULT_MAX_ENUM};
  XrResult mQueryLayersResult {XR_RESULT_MAX_ENUM};

  std::vector<std::string> mEnabledLayerNames;

  // It's possible for runtimes to modify the environment variables,
  // which can disable API layers
  std::map<std::string, std::string> mEnvironmentVariablesBeforeLoader;
  std::map<std::string, std::string> mEnvironmentVariablesAfterLoader;

 private:
  static Architecture GetBuildArchitecture();
};

void from_json(const nlohmann::json&, LoaderData&);
void to_json(nlohmann::json&, const LoaderData&);

}// namespace FredEmmott::OpenXRLayers