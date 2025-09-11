// Copyright 2025 Fred Emmott <fred@fredemmott.com>
// SPDX-License-Identifier: MIT
#include "Platform.hpp"

#include <nlohmann/json.hpp>

#include <fstream>

#include "APILayer.hpp"

namespace FredEmmott::OpenXRLayers {

namespace {
std::optional<Runtime> GetRuntimeFromPath(const std::filesystem::path& path) {
  using enum Runtime::ManifestError;

  if (path.empty()) {
    return {};
  }

  try {
    if (!std::filesystem::exists(path)) {
      return Runtime {path, std::unexpected {FileNotFound}};
    }

    std::ifstream f(path);
    if (!f) {
      return Runtime {path, std::unexpected {FileNotReadable}};
    }

    const auto json = nlohmann::json::parse(f);
    if (json.contains("runtime") && json.at("runtime").contains("name")) {
      return Runtime {path, json.at("runtime").at("name")};
    }

    return Runtime {path, std::unexpected {FieldNotPresent}};
  } catch (const std::filesystem::filesystem_error& e) {
    return Runtime {path, std::unexpected {FileNotReadable}};
  } catch (const nlohmann::json::exception& e) {
    return Runtime {path, std::unexpected {InvalidJson}};
  }
}
}// namespace

Platform::Platform() = default;
Platform::~Platform() = default;

std::optional<Runtime> Platform::Get32BitRuntime() {
  return GetRuntimeFromPath(Get32BitRuntimePath());
}

std::optional<Runtime> Platform::Get64BitRuntime() {
  return GetRuntimeFromPath(Get64BitRuntimePath());
}

}// namespace FredEmmott::OpenXRLayers
