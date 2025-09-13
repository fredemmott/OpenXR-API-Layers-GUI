// Copyright 2025 Fred Emmott <fred@fredemmott.com>
// SPDX-License-Identifier: MIT
#include "LoaderData.hpp"

#include <Windows.h>

#include <nlohmann/json.hpp>

#include <iostream>
#include <utility>

#include "LoaderData.hpp"
#include "Platform.hpp"

namespace FredEmmott::OpenXRLayers {

void to_json(nlohmann::json& j, const LoaderData& data) {
  j.update(
    nlohmann::json {
      {"queryExtensionsResult", std::to_underlying(data.mQueryLayersResult)},
      {"queryLayersResult", std::to_underlying(data.mQueryLayersResult)},
      {"enabledLayerNames", data.mEnabledLayerNames},
      {"environmentVariables",
       {
         {"beforeLoader", data.mEnvironmentVariablesBeforeLoader},
         {"afterLoader", data.mEnvironmentVariablesAfterLoader},
       }},
    });
}

void from_json(const nlohmann::json& j, LoaderData& data) {
  data.mQueryLayersResult = static_cast<XrResult>(j["queryLayersResult"]);
  data.mQueryExtensionsResult
    = static_cast<XrResult>(j["queryExtensionsResult"]);
  j.at("enabledLayerNames").get_to(data.mEnabledLayerNames);
  j.at("environmentVariables")
    .at("beforeLoader")
    .get_to(data.mEnvironmentVariablesBeforeLoader);
  j.at("environmentVariables")
    .at("afterLoader")
    .get_to(data.mEnvironmentVariablesAfterLoader);
}

}// namespace FredEmmott::OpenXRLayers