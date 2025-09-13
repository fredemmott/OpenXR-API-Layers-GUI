// Copyright 2025 Fred Emmott <fred@fredemmott.com>
// SPDX-License-Identifier: MIT
#pragma once
#include <nlohmann/json_fwd.hpp>
#include <openxr/openxr.h>

#include <string>
#include <system_error>
#include <unordered_set>
#include <variant>

namespace FredEmmott::OpenXRLayers {

struct LoaderData {
  struct PipeCreationError {
    std::error_code mError;
  };
  struct PipeAttributeError {
    std::error_code mError;
  };
  struct CanNotFindExecutableError {
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
    PipeCreationError,
    PipeAttributeError,
    CanNotFindExecutableError,
    CanNotSpawnError,
    BadExitCodeError,
    InvalidJSONError>;

  XrResult mQueryExtensionsResult {XR_RESULT_MAX_ENUM};
  XrResult mQueryLayersResult {XR_RESULT_MAX_ENUM};

  std::unordered_set<std::string> mEnabledLayerNames;

  // It's possible for runtimes to modify the environment variables,
  // which can disable API layers
  std::unordered_set<std::string> mEnvironmentVariablesBeforeLoader;
  std::unordered_set<std::string> mEnvironmentVariablesAfterLoader;
};

void from_json(const nlohmann::json&, LoaderData&);
void to_json(nlohmann::json&, const LoaderData&);

}// namespace FredEmmott::OpenXRLayers