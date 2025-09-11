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

LoaderData QueryLoaderDataInCurrentProcess() {
  LoaderData ret {
    .mEnvironmentVariablesBeforeLoader
    = Platform::Get().GetEnvironmentVariableNames(),
  };

  // We don't care about the extensions, but enumerating them can load the
  // runtime DLL, which can call `setenv()` and change the rest
  uint32_t propertyCount {};
  ret.mQueryExtensionsResult = xrEnumerateInstanceExtensionProperties(
    nullptr, 0, &propertyCount, nullptr);

  uint32_t layerCount {};
  ret.mQueryLayersResult
    = xrEnumerateApiLayerProperties(0, &layerCount, nullptr);
  ret.mEnvironmentVariablesAfterLoader
    = Platform::Get().GetEnvironmentVariableNames();
  if (XR_FAILED(ret.mQueryLayersResult)) {
    return ret;
  }

  std::vector<XrApiLayerProperties> layers(
    layerCount, {XR_TYPE_API_LAYER_PROPERTIES});
  ret.mQueryLayersResult
    = xrEnumerateApiLayerProperties(layerCount, &layerCount, layers.data());

  for (auto&& layer: layers) {
    ret.mEnabledLayerNames.emplace_back(layer.layerName);
  }

  return ret;
}

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

void LoaderMain() {
  const auto data = QueryLoaderDataInCurrentProcess();
  const nlohmann::json json(data);

  std::cout << std::setw(2) << json << std::endl;
}

}// namespace FredEmmott::OpenXRLayers