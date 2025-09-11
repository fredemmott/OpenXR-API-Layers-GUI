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

std::expected<std::string, Runtime::ManifestError> GetRuntimeName(
  const std::filesystem::path& path) {
  using enum Runtime::ManifestError;
  try {
    if (!std::filesystem::exists(path)) {
      return std::unexpected {FileNotFound};
    }

    std::ifstream f(path);
    if (!f) {
      return std::unexpected {FileNotReadable};
    }

    const auto json = nlohmann::json::parse(f);
    if (json.contains("runtime") && json.at("runtime").contains("name")) {
      return json.at("runtime").at("name");
    }

    return std::unexpected {FieldNotPresent};
  } catch (const std::filesystem::filesystem_error&) {
    return std::unexpected {FileNotReadable};
  } catch (const nlohmann::json::exception&) {
    return std::unexpected {InvalidJson};
  }
}

}// namespace

Runtime::Runtime(const std::filesystem::path& path)
  : mPath(path),
    mName(GetRuntimeName(path)) {}

AvailableRuntime::AvailableRuntime(
  const std::filesystem::path& path,
  const Discoverability discoverability)
  : Runtime(path),
    mDiscoverability(discoverability) {}

Platform::Platform() = default;
Platform::~Platform() = default;

std::optional<Runtime> Platform::Get32BitRuntime() {
  return GetRuntimeFromPath(Get32BitRuntimePath());
}

std::optional<Runtime> Platform::Get64BitRuntime() {
  return GetRuntimeFromPath(Get64BitRuntimePath());
}

}// namespace FredEmmott::OpenXRLayers
