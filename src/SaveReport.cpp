// Copyright 2023 Fred Emmott <fred@fredemmott.com>
// SPDX-License-Identifier: ISC

#include "SaveReport.hpp"

#include <fmt/chrono.h>
#include <fmt/format.h>

#include <chrono>
#include <fstream>
#include <ranges>

#include "APILayerStore.hpp"
#include "Config.hpp"
#include "Linter.hpp"

namespace FredEmmott::OpenXRLayers {

static std::string GenerateReportText(const APILayerStore* store) {
  auto ret = std::format(
    "\n--------------------------------\n"
    "{}\n"
    "--------------------------------",
    store->GetDisplayName());
  const auto layers = store->GetAPILayers();
  if (layers.empty()) {
    ret += "\nNo layers.";
    return ret;
  }

  const auto errors = RunAllLinters(store, layers);

  for (const auto& layer: layers) {
    using Value = APILayer::Value;
    std::string_view value;
    switch (layer.mValue) {
      case Value::Enabled:
        value = Config::GLYPH_ENABLED;
        break;
      case Value::Disabled:
        value = Config::GLYPH_DISABLED;
        break;
      default:
        value = Config::GLYPH_ERROR;
        break;
    }
    ret += std::format("\n{} {}", value, layer.mJSONPath.string());

    const APILayerDetails details {layer.mJSONPath};
    if (details.mState != APILayerDetails::State::Loaded) {
      ret += fmt::format(
        "\n\t- {} {}", Config::GLYPH_ERROR, details.StateAsString());
    } else {
      if (!details.mName.empty()) {
        ret += fmt::format("\n\tName: {}", details.mName);
      }

      if (details.mLibraryPath.empty()) {
        ret += fmt::format(
          "\n\tLibrary path: {} No library path in JSON file",
          Config::GLYPH_ERROR);
      } else {
        ret
          += fmt::format("\n\tLibrary path: {}", details.mLibraryPath.string());
      }

      if (!details.mDescription.empty()) {
        ret += fmt::format("\n\tDescription: {}", details.mDescription);
      }

      if (!details.mFileFormatVersion.empty()) {
        ret += fmt::format(
          "\n\tFile format version: {}", details.mFileFormatVersion);
      }

      if (!details.mExtensions.empty()) {
        ret += "\n\tExtensions:";
        for (const auto& ext: details.mExtensions) {
          ret
            += fmt::format("\n\t\t- {} (version {})", ext.mName, ext.mVersion);
        }
      }
    }

    auto layerErrors
      = std::ranges::filter_view(errors, [layer](const auto& error) {
          return error->GetAffectedLayers().contains(layer.mJSONPath);
        });

    if (layerErrors.empty()) {
      ret += "\n\tNo errors.";
    } else {
      ret += "\n\tErrors:";
      for (const auto& error: layerErrors) {
        ret += fmt::format(
          "\n\t\t- {} {}", Config::GLYPH_ERROR, error->GetDescription());
      }
    }
  }
  return ret;
}

void SaveReport(const std::filesystem::path& path) {
  auto text = std::format(
    "OpenXR API Layers GUI v{}\n"
    "Reported generated at {:%Y-%m-%d %H:%M:%S}",
    Config::BUILD_VERSION,
    std::chrono::zoned_time(
      std::chrono::current_zone(), std::chrono::system_clock::now()));

  for (const auto store: APILayerStore::Get()) {
    text += GenerateReportText(store);
  }

  std::ofstream(path, std::ios::binary | std::ios::out | std::ios::trunc)
    .write(text.data(), text.size());
}

}// namespace FredEmmott::OpenXRLayers