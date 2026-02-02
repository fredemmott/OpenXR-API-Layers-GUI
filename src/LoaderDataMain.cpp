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

  // We (mostly) don't care about the extensions, but enumerating them can load
  // runtime DLL, which can call `setenv()` and change the rest. However, while
  // they're currently unused in the linters and UI, we do include them in the
  // report
  uint32_t propertyCount {};
  ret.mQueryExtensionsResult = xrEnumerateInstanceExtensionProperties(
    nullptr, 0, &propertyCount, nullptr);
  if (XR_SUCCEEDED(ret.mQueryExtensionsResult)) {
    std::vector<XrExtensionProperties> extensions(
      propertyCount, {XR_TYPE_EXTENSION_PROPERTIES});
    ret.mQueryExtensionsResult = xrEnumerateInstanceExtensionProperties(
      nullptr, propertyCount, &propertyCount, extensions.data());
    if (XR_SUCCEEDED(ret.mQueryExtensionsResult)) {
      for (auto&& ext: extensions) {
        ret.mAvailableExtensionNames.emplace_back(ext.extensionName);
      }
    }
  }

  uint32_t layerCount {};
  ret.mQueryLayersResult
    = xrEnumerateApiLayerProperties(0, &layerCount, nullptr);
  if (XR_SUCCEEDED(ret.mQueryLayersResult)) {
    std::vector<XrApiLayerProperties> layers(
      layerCount, {XR_TYPE_API_LAYER_PROPERTIES});
    ret.mQueryLayersResult
      = xrEnumerateApiLayerProperties(layerCount, &layerCount, layers.data());
    if (XR_SUCCEEDED(ret.mQueryLayersResult)) {
      for (auto&& layer: layers) {
        ret.mEnabledLayerNames.emplace_back(layer.layerName);
      }
    }
  }

  ret.mEnvironmentVariablesAfterLoader
    = Platform::Get().GetEnvironmentVariables();
  return ret;
}

void LoaderDataMain() {
  const auto data = QueryLoaderDataInCurrentProcess();
  const nlohmann::json json(data);

  std::cout << std::setw(2) << json << std::endl;
}

}// namespace FredEmmott::OpenXRLayers