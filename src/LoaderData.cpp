// Copyright 2025 Fred Emmott <fred@fredemmott.com>
// SPDX-License-Identifier: MIT
#include "LoaderData.hpp"

#include <Windows.h>

#include <wil/resource.h>

#include <nlohmann/json.hpp>

#include <iostream>
#include <utility>

namespace FredEmmott::OpenXRLayers {

namespace {
int WideCharToUTF8(
  wchar_t const* in,
  const std::size_t inCharCount,
  char* out,
  const std::size_t outByteCount) {
  return WideCharToMultiByte(
    CP_UTF8,
    0,
    in,
    static_cast<int>(inCharCount),
    out,
    static_cast<int>(outByteCount),
    nullptr,
    nullptr);
}

std::vector<std::string> GetEnvironmentVariableNames() {
  std::vector<std::string> ret;
  wil::unique_environstrings_ptr env {GetEnvironmentStringsW()};
  std::string buf;

  for (auto it = env.get(); (it && *it); it += (wcslen(it) + 1)) {
    const auto nameEnd = std::wstring_view {it}.find(L'=');
    if (nameEnd == 0 || nameEnd == std::wstring_view::npos) {
      continue;
    }

    const auto byteCount = WideCharToUTF8(it, nameEnd, nullptr, 0);
    buf.resize_and_overwrite(byteCount + 1, [=](auto p, const auto size) {
      return WideCharToUTF8(it, nameEnd, p, size);
    });
    ret.emplace_back(buf.data(), static_cast<std::size_t>(byteCount));
  }
  std::ranges::sort(ret);
  return ret;
}
}// namespace

LoaderData LoaderData::Get() {
  LoaderData ret {
    .mEnvironmentVariablesBeforeLoader = GetEnvironmentVariableNames(),
  };

  // We don't care about the extensions, but enumerating them can load the
  // runtime DLL, which can call `setenv()` and change the rest
  uint32_t propertyCount {};
  ret.mQueryExtensionsResult = xrEnumerateInstanceExtensionProperties(
    nullptr, 0, &propertyCount, nullptr);

  uint32_t layerCount {};
  ret.mQueryLayersResult
    = xrEnumerateApiLayerProperties(0, &layerCount, nullptr);
  ret.mEnvironmentVariablesAfterLoader = GetEnvironmentVariableNames();
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

void LoaderMain() {
  const auto data = LoaderData::Get();
  const nlohmann::json json(data);

  std::cout << std::setw(2) << json << std::endl;
}

}// namespace FredEmmott::OpenXRLayers