// Copyright 2025 Fred Emmott <fred@fredemmott.com>
// SPDX-License-Identifier: MIT
#pragma once
#include <nlohmann/json_fwd.hpp>
#include <openxr/openxr.h>

#include <string>
#include <vector>

namespace FredEmmott::OpenXRLayers {

struct LoaderData {
  XrResult mQueryExtensionsResult {XR_RESULT_MAX_ENUM};
  XrResult mQueryLayersResult {XR_RESULT_MAX_ENUM};

  std::vector<std::string> mEnabledLayerNames;

  // It's possible for runtimes to modify the environment variables,
  // which can disable API layers
  std::vector<std::string> mEnvironmentVariablesBeforeLoader;
  std::vector<std::string> mEnvironmentVariablesAfterLoader;
};

void from_json(const nlohmann::json&, LoaderData&);
void to_json(nlohmann::json&, const LoaderData&);

void LoaderMain();

}// namespace FredEmmott::OpenXRLayers