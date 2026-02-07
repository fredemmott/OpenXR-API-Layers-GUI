// Copyright 2025 Fred Emmott <fred@fredemmott.com>
// SPDX-License-Identifier: MIT
#include "Platform.hpp"

#include <nlohmann/json.hpp>

#include <fstream>

#include "APILayer.hpp"

namespace FredEmmott::OpenXRLayers {

namespace {
std::optional<Runtime> GetRuntimeFromPath(const std::filesystem::path& path) {
  if (path.empty()) {
    return {};
  }
  return Runtime(path);
}

std::expected<Runtime::ManifestData, Runtime::ManifestData::Error>
GetRuntimeManifestData(const std::filesystem::path& path) {
  using enum Runtime::ManifestData::Error;
  try {
    if (!std::filesystem::exists(path)) {
      return std::unexpected {FileNotFound};
    }

    std::ifstream f(path);
    if (!f) {
      return std::unexpected {FileNotReadable};
    }

    const auto json = nlohmann::json::parse(f);
    if (!json.contains("runtime")) {
      return std::unexpected {InvalidJson};
    }
    const auto& data = json.at("runtime");

    Runtime::ManifestData ret {};
    if (data.contains("name")) {
      ret.mName = data.at("name");
    }
    if (data.contains("library_path")) {
      const std::filesystem::path libraryPath {
        data.at("library_path").get<std::string>()};
      if (libraryPath.is_absolute()) {
        ret.mLibraryPath = libraryPath;
      } else {
        ret.mLibraryPath = weakly_canonical(path.parent_path() / libraryPath);
      }
    }

    if ((!ret.mLibraryPath.empty()) && exists(ret.mLibraryPath)) {
      ret.mLibrarySignature
        = Platform::Get().GetSharedLibrarySignature(ret.mLibraryPath);
    }

    return ret;
  } catch (const std::filesystem::filesystem_error&) {
    return std::unexpected {FileNotReadable};
  } catch (const nlohmann::json::exception&) {
    return std::unexpected {InvalidJson};
  }
}

}// namespace

Runtime::Runtime(const std::filesystem::path& path)
  : mPath(path),
    mManifestData(GetRuntimeManifestData(path)) {}

AvailableRuntime::AvailableRuntime(
  const std::filesystem::path& path,
  const Discoverability discoverability)
  : Runtime(path),
    mDiscoverability(discoverability) {}

Platform::Platform() = default;
Platform::~Platform() = default;

std::optional<Runtime> Platform::GetActiveRuntime(const Architecture arch) {
  return GetRuntimeFromPath(GetActiveRuntimePath(arch));
}

}// namespace FredEmmott::OpenXRLayers
