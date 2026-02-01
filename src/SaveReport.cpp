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
#include "Platform.hpp"

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
    ret += std::format("\n{} {}", value, layer.GetKey().mValue);

    if (!layer.mManifestPath.empty()) {
      const APILayerDetails details {layer.mManifestPath};
      if (details.mState != APILayerDetails::State::Loaded) {
        ret += fmt::format(
          "\n\t- {} {}", Config::GLYPH_ERROR, details.StateAsString());
      } else {
        ret += fmt::format(
          "\n\tManifest JSON last modified at: {}",
          std::chrono::clock_cast<std::chrono::system_clock>(
            std::chrono::time_point_cast<std::chrono::seconds>(
              details.mManifestFilesystemChangeTime)));

        if (!details.mName.empty()) {
          ret += fmt::format("\n\tName: {}", details.mName);
        }

        if (details.mLibraryPath.empty()) {
          ret += fmt::format(
            "\n\tLibrary path: {} No library path in JSON file",
            Config::GLYPH_ERROR);
        } else {
          ret += fmt::format(
            "\n\tLibrary path: {}", details.mLibraryPath.string());

          ret += fmt::format(
            "\n\tLibrary last modified at: {}",
            details.mLibraryPath.extension().string(),
            std::chrono::clock_cast<std::chrono::system_clock>(
              std::chrono::time_point_cast<std::chrono::seconds>(
                details.mLibraryFilesystemChangeTime)));

          if (details.mSignature) {
            ret += fmt::format(
              "\n\tSigned by: {}"
              "\n\tSigned at: {}",
              details.mSignature->mSignedBy,
              std::chrono::time_point_cast<std::chrono::seconds>(
                details.mSignature->mSignedAt));
            // No need for an 'else' - linters already report warnings when
            // appropriate
          }
        }

        if (!details.mImplementationVersion.empty()) {
          ret += fmt::format(
            "\n\tImplementation version: {}", details.mImplementationVersion);
        }

        if (!details.mAPIVersion.empty()) {
          ret += fmt::format("\n\tOpenXR API version: {}", details.mAPIVersion);
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
            ret += fmt::format(
              "\n\t\t- {} (version {})", ext.mName, ext.mVersion);
          }
        }
      }
    }

    auto layerErrors
      = std::ranges::filter_view(errors, [layer](const auto& error) {
          return error->GetAffectedLayers().contains(layer);
        });

    if (layerErrors.empty()) {
      if (layer.IsEnabled()) {
        ret += "\n\tNo errors.";
      } else {
        ret += "\n\tNo errors, however most linters were skipped because the layer is disabled.";
      }
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

static std::string GenerateActiveRuntimeText(
  const uint8_t bitness,
  const std::optional<Runtime>& runtime) {
  if (!runtime) {
    return std::format("❌ Active {}-bit runtime: NONE\n", bitness);
  }

  if (!runtime->mName) {
    if (runtime->mName.error() != Runtime::ManifestError::FieldNotPresent) {
      return std::format(
        "🚨 Active {}-bit runtime: CORRUPTED - {}\n",
        bitness,
        runtime->mPath.string());
    }
    return std::format(
      "✅ Active {}-bit runtime: {}\n", bitness, runtime->mPath.string());
  }

  return std::format(
    "✅ Active {}-bit runtime: \"{}\" - {}\n",
    bitness,
    runtime->mName.value(),
    runtime->mPath.string());
}

static std::string GenerateAvailableRuntimesText(
  const uint8_t bitness,
  const std::vector<AvailableRuntime>& runtimes) {
  auto ret = std::format("\nAvailable {}-bit runtimes:\n", bitness);
  if (runtimes.empty()) {
    return ret + "  NONE\n";
  }

  for (auto&& runtime: runtimes) {
    if (runtime.mName) {
      ret += fmt::format(
        "  - \"{}\" - {}", runtime.mName.value(), runtime.mPath.string());
    } else {
      using enum Runtime::ManifestError;
      switch (runtime.mName.error()) {
        case FieldNotPresent:
          ret += fmt::format("  - {}", runtime.mPath.string());
          break;
        case FileNotFound:
          ret += fmt::format("  - ❌ FILE MISSING: {}", runtime.mPath.string());
          break;
        case FileNotReadable:
        case InvalidJson:
          ret += fmt::format(
            "  - ❌ FILE NOT READABLE: {}", runtime.mPath.string());
          break;
      }
    }

    switch (runtime.mDiscoverability) {
      case AvailableRuntime::Discoverability::Discoverable:
        ret += " (discoverable)\n";
        break;
      case AvailableRuntime::Discoverability::Hidden:
        ret += " (disabled)\n";
        break;
      case AvailableRuntime::Discoverability::Win32_NotDWORD:
        ret += " (🚨 NOT A DWORD)\n";
        break;
    }
  }

  return ret;
}

void SaveReport(const std::filesystem::path& path) {
  auto text = std::format(
    "OpenXR API Layers GUI v{}\n"
    "Reported generated at {:%Y-%m-%d %H:%M:%S}\n\n",
    Config::BUILD_VERSION,
    std::chrono::zoned_time(
      std::chrono::current_zone(), std::chrono::system_clock::now()));

  auto& platform = Platform::Get();
  text += GenerateActiveRuntimeText(64, platform.Get64BitRuntime());
  text += GenerateActiveRuntimeText(32, platform.Get32BitRuntime());

  text
    += GenerateAvailableRuntimesText(64, platform.GetAvailable64BitRuntimes());
  text
    += GenerateAvailableRuntimesText(32, platform.GetAvailable64BitRuntimes());

  for (const auto store: APILayerStore::Get()) {
    text += GenerateReportText(store);
  }

  std::ofstream(path, std::ios::binary | std::ios::out | std::ios::trunc)
    .write(text.data(), text.size());
}

}// namespace FredEmmott::OpenXRLayers