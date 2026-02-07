// Copyright 2023 Fred Emmott <fred@fredemmott.com>
// SPDX-License-Identifier: ISC

#include "SaveReport.hpp"

#include <fmt/chrono.h>
#include <fmt/format.h>
#include <nlohmann/json.hpp>

#include <chrono>
#include <fstream>
#include <ranges>

#include "APILayerStore.hpp"
#include "Config.hpp"
#include "Linter.hpp"
#include "Platform.hpp"

namespace FredEmmott::OpenXRLayers {

namespace {
template <class... Ts>
struct overloaded : Ts... {
  using Ts::operator()...;
};
}// namespace

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
  const Architecture arch,
  const std::optional<Runtime>& runtime) {
  const auto archName = magic_enum::enum_name(arch);
  if (!runtime) {
    return std::format("❌ Active {} runtime: NONE\n", archName);
  }
  const auto manifest = runtime->mManifestData;
  if (!manifest) {
    return std::format(
      "❌ Invalid {} manifest at `{}`: {}\n",
      archName,
      runtime->mPath.string(),
      magic_enum::enum_name(manifest.error()));
  }

  std::string out = runtime->mPath.string();
  if (manifest->mName != runtime->mPath) {
    out += std::format(" ()", manifest->mName);
  }

  bool ok = true;
  if (manifest->mLibrarySignature) {
    out += fmt::format(
      " (signed by '{}')", manifest->mLibrarySignature->mSignedBy);
  } else {
    ok = false;
    out += fmt::format(
      " - 🚨 bad signature: {}",
      magic_enum::enum_name(manifest->mLibrarySignature.error()));
  }

  return std::format("{} {}\n", ok ? "✅" : "🚨", out);
}

static std::string GenerateAvailableRuntimesText(
  const Architecture arch,
  const std::vector<AvailableRuntime>& runtimes) {
  const auto archName = magic_enum::enum_name(arch);
  auto ret = std::format("\nAvailable {} runtimes:\n", archName);
  if (runtimes.empty()) {
    return ret + "  NONE\n";
  }

  for (auto&& runtime: runtimes) {
    if (!runtime.mManifestData) {
      ret += fmt::format(
        "  - ❌ {}: 🚨 {}\n",
        runtime.mPath.string(),
        magic_enum::enum_name(runtime.mManifestData.error()));
      continue;
    }

    ret += fmt::format(
      "  - \"{}\" - {}", runtime.mManifestData->mName, runtime.mPath.string());

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

static std::string GenerateLoaderDataText(
  Platform& platform,
  const Architecture arch,
  const std::chrono::steady_clock::time_point deadline) {
  const auto result = platform.WaitForLoaderData(arch, deadline);
  auto ret = std::format(
    "\n\n--------------------------------\n"
    "Loader Data ({})\n"
    "--------------------------------\n\n",
    magic_enum::enum_name(arch));
  if (!result) {
    return std::format(
      "{}❌ {}",
      ret,
      std::visit(
        overloaded {
          [](const LoaderData::PendingError&) -> std::string {
            return "Still Pending";
          },
          [](const LoaderData::PipeCreationError& e) {
            return std::format("Pipe creation error: {}", e.mError.message());
          },
          [](const LoaderData::PipeAttributeError& e) {
            return std::format("Pipe attribute error: {}", e.mError.message());
          },
          [](const LoaderData::CanNotFindCurrentExecutableError& e) {
            return std::format(
              "Can not find current executable: {}", e.mError.message());
          },
          [](const LoaderData::CanNotFindHelperExecutableError& e) {
            return std::format(
              "Helper executable does not exist: {}", e.mPath.string());
          },
          [](const LoaderData::UnsignedHelperError& e) {
            return std::format(
              "⚠️ Invalid signature on loader data helper: {}",
              magic_enum::enum_name(e.mError));
          },
          [](const LoaderData::CanNotSpawnError& e) {
            return std::format(
              "Subprocess creation failed: {}", e.mError.message());
          },
          [](const LoaderData::BadExitCodeError& e) {
            return std::format(
              "Bad exit code: {} ({:#010x})",
              e.mExitCode,
              std::bit_cast<uint32_t>(e.mExitCode));
            ;
          },
          [](const LoaderData::InvalidJSONError& e) {
            return std::format("Invalid JSON: {}", e.mExplanation);
          },
        },
        result.error()));
  }

  const auto& data = *result;
  nlohmann::json json = data;
  json.erase("environmentVariables");

  const auto& before = data.mEnvironmentVariablesBeforeLoader;
  const auto& after = data.mEnvironmentVariablesAfterLoader;
  std::vector<std::string> allEnvVars;
  allEnvVars.append_range(std::views::keys(before));
  allEnvVars.append_range(std::views::keys(after));
  std::ranges::sort(allEnvVars);
  {
    auto [first, last] = std::ranges::unique(allEnvVars);
    allEnvVars.erase(first, last);
  }

  auto outEnvVars = nlohmann::json::array();

  for (auto&& key: allEnvVars) {
    const bool censor = (!key.starts_with("XR_")) && (!key.contains("_XR_"));

    if (!after.contains(key)) {
      assert(before.contains(key));
      const auto value = censor ? "[*****]" : before.at(key);
      outEnvVars.emplace_back(
        std::format("⚠️➖ unset by runtime: {}={}", key, value));
      continue;
    }

    if (!before.contains(key)) {
      assert(after.contains(key));
      const auto value = censor ? "[*****]" : after.at(key);
      outEnvVars.emplace_back(
        std::format("⚠️➕ added by runtime: {}={}", key, value));
      continue;
    }

    const auto beforeValue = censor ? "[*****]" : before.at(key);
    if (before.at(key) == after.at(key)) {
      outEnvVars.emplace_back(std::format("{}={}", key, beforeValue));
      continue;
    }

    const auto afterValue = censor ? "[*****]" : after.at(key);
    outEnvVars.emplace_back(
      std::format(
        "⚠️🔄 modified by runtime: -{}={} +{}={}",
        key,
        beforeValue,
        key,
        afterValue));
  }

  json["environmentVariables"] = outEnvVars;

  return ret + json.dump(2);
}

void SaveReport(const std::filesystem::path& path) {
  const auto text = GenerateReport();
  std::ofstream(path, std::ios::binary | std::ios::out | std::ios::trunc)
    .write(text.data(), text.size());
}

std::string GenerateReport() {
  auto text = std::format(
    "OpenXR API Layers GUI v{}\n"
    "Reported generated at {:%Y-%m-%d %H:%M:%S}\n\n",
    Config::BUILD_VERSION,
    std::chrono::zoned_time(
      std::chrono::current_zone(), std::chrono::system_clock::now()));

  auto& platform = Platform::Get();
  for (const auto arch: platform.GetArchitectures().enumerate()) {
    text += GenerateActiveRuntimeText(arch, platform.GetActiveRuntime(arch));
  }
  for (const auto arch: platform.GetArchitectures().enumerate()) {
    text += GenerateAvailableRuntimesText(
      arch, platform.GetAvailableRuntimes(arch));
  }

  for (const auto store: APILayerStore::Get()) {
    text += GenerateReportText(store);
  }

  const auto deadline
    = std::chrono::steady_clock::now() + std::chrono::seconds(10);
  for (const auto arch: platform.GetArchitectures().enumerate()) {
    text += GenerateLoaderDataText(platform, arch, deadline);
  }

  return text;
}

}// namespace FredEmmott::OpenXRLayers