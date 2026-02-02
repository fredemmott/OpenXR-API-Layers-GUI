// Copyright 2025 Fred Emmott <fred@fredemmott.com>
// SPDX-License-Identifier: MIT
#pragma once
#include <boost/signals2.hpp>

#include <expected>
#include <filesystem>
#include <functional>
#include <optional>
#include <unordered_set>

#include <imgui.h>

#include "APILayerSignature.hpp"
#include "Architectures.hpp"
#include "LoaderData.hpp"

namespace FredEmmott::OpenXRLayers {
class APILayerStore;
class ReadWriteAPILayerStore;

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
  std::optional<Runtime> GetActiveRuntime();

  virtual void GUIMain(std::function<void()> drawFrame) = 0;

  /// Unlike `std::filesystem::last_write_time()`, this should
  /// return the actual time the file was modified on disk, e.g. when it
  /// was extracted/installed
  virtual std::filesystem::file_time_type GetFileChangeTime(
    const std::filesystem::path& path) = 0;

  virtual std::expected<APILayerSignature, APILayerSignature::Error>
  GetAPILayerSignature(const std::filesystem::path&) = 0;
  virtual std::expected<LoaderData, LoaderData::Error> GetLoaderData(
    Architecture) = 0;
  virtual std::expected<LoaderData, LoaderData::Error> WaitForLoaderData(
    Architecture,
    std::chrono::steady_clock::time_point timeout) = 0;
  virtual std::vector<std::filesystem::path> GetNewAPILayerJSONPaths() = 0;
  virtual std::optional<std::filesystem::path> GetExportFilePath() = 0;
  virtual std::unordered_set<std::string> GetEnvironmentVariableNames() = 0;
  virtual std::vector<std::string> GetEnabledExplicitAPILayers() = 0;
  virtual float GetDPIScaling() = 0;

  virtual std::vector<AvailableRuntime> GetAvailable32BitRuntimes() = 0;
  virtual std::vector<AvailableRuntime> GetAvailable64BitRuntimes() = 0;

  // Use OS/environment equivalent to Explorer
  virtual void ShowFolderContainingFile(const std::filesystem::path&) = 0;

  static constexpr Architecture GetBuildArchitecture();

  virtual Architectures GetArchitectures() const = 0;
  // Plural in case we ever support macOS, which has 'fat dylibs'
  virtual Architectures GetSharedLibraryArchitectures(
    const std::filesystem::path&) const = 0;
  virtual std::optional<std::vector<std::filesystem::path>> GetOverridePaths()
    const = 0;

  Platform(const Platform&) = delete;
  Platform(Platform&&) = delete;
  Platform& operator=(const Platform&) = delete;
  Platform& operator=(Platform&&) = delete;

  boost::signals2::scoped_connection OnLoaderData(
    std::function<void()> callback) noexcept {
    return mOnLoaderDataSignal.connect(std::move(callback));
  }

 protected:
  boost::signals2::signal<void()> mOnLoaderDataSignal;

  Platform();
  virtual std::filesystem::path Get32BitRuntimePath() = 0;
  virtual std::filesystem::path Get64BitRuntimePath() = 0;
};

constexpr Architecture Platform::GetBuildArchitecture() {
#if defined(_M_X64) || defined(__x86_64__)
  return Architecture::x64;
#elif defined(_M_IX86) || defined(__i386__)
  return Architecture::x86;
#else
#error "Unsupported architecture"
#endif
}

}// namespace FredEmmott::OpenXRLayers