// Copyright 2025 Fred Emmott <fred@fredemmott.com>
// SPDX-License-Identifier: MIT
#include "LoaderDataMain.hpp"

#include <nlohmann/json.hpp>

#include <iostream>

#include "LoaderData.hpp"
#include "Platform.hpp"

namespace FredEmmott::OpenXRLayers {

static LoaderData QueryLoaderDataInCurrentProcess() {
  LoaderData ret {
    .mEnvironmentVariablesBeforeLoader
    = Platform::Get().GetEnvironmentVariables(),
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
    = Platform::Get().GetEnvironmentVariables();
  if (XR_FAILED(ret.mQueryLayersResult)) {
    return ret;
  }

  std::vector<XrApiLayerProperties> layers(
    layerCount, {XR_TYPE_API_LAYER_PROPERTIES});
  ret.mQueryLayersResult
    = xrEnumerateApiLayerProperties(layerCount, &layerCount, layers.data());

  for (auto&& layer: layers) {
    ret.mEnabledLayerNames.emplace(layer.layerName);
  }

  return ret;
}

void LoaderDataMain() {
  const auto data = QueryLoaderDataInCurrentProcess();
  const nlohmann::json json(data);

  std::cout << std::setw(2) << json << std::endl;
}

}// namespace FredEmmott::OpenXRLayers