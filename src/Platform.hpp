// Copyright 2025 Fred Emmott <fred@fredemmott.com>
// SPDX-License-Identifier: MIT
#pragma once
#include <expected>
#include <filesystem>
#include <functional>
#include <optional>
#include <unordered_set>

#include <imgui.h>

namespace FredEmmott::OpenXRLayers {
class APILayerStore;
class ReadWriteAPILayerStore;
struct LoaderData;

struct DPIChangeInfo {
  float mDPIScaling {};
  std::optional<ImVec2> mRecommendedSize;
};

struct Runtime {
  enum class ManifestError {
    FileNotFound,
    FileNotReadable,
    InvalidJson,
    FieldNotPresent,
  };

  Runtime() = delete;
  explicit Runtime(const std::filesystem::path& path);

  std::filesystem::path mPath;
  std::expected<std::string, ManifestError> mName;
};

struct AvailableRuntime : Runtime {
  enum class Discoverability {
    Discoverable,
    Hidden,
    Win32_NotDWORD,
  };

  AvailableRuntime() = delete;
  AvailableRuntime(const std::filesystem::path& path, Discoverability);

  Discoverability mDiscoverability;
};

// Platform-specific functions implemented in PlatformGUI_P*.cpp
class Platform {
 public:
  static Platform& Get();
  virtual ~Platform();

  std::optional<Runtime> Get32BitRuntime();
  std::optional<Runtime> Get64BitRuntime();

  virtual void GUIMain(std::function<void()> drawFrame) = 0;

  virtual std::expected<LoaderData, std::string> GetLoaderData() = 0;
  virtual std::vector<std::filesystem::path> GetNewAPILayerJSONPaths() = 0;
  virtual std::optional<std::filesystem::path> GetExportFilePath() = 0;
  virtual std::unordered_set<std::string> GetEnvironmentVariableNames() = 0;
  virtual float GetDPIScaling() = 0;

  virtual std::vector<AvailableRuntime> GetAvailable32BitRuntimes() = 0;
  virtual std::vector<AvailableRuntime> GetAvailable64BitRuntimes() = 0;

  // Use OS/environment equivalent to Explorer
  virtual void ShowFolderContainingFile(const std::filesystem::path&) = 0;

  Platform(const Platform&) = delete;
  Platform(Platform&&) = delete;
  Platform& operator=(const Platform&) = delete;
  Platform& operator=(Platform&&) = delete;

 protected:
  Platform();
  virtual std::filesystem::path Get32BitRuntimePath() = 0;
  virtual std::filesystem::path Get64BitRuntimePath() = 0;
};
}// namespace FredEmmott::OpenXRLayers