// Copyright 2025 Fred Emmott <fred@fredemmott.com>
// SPDX-License-Identifier: MIT
#pragma once
#include <expected>
#include <filesystem>
#include <functional>
#include <optional>

#include <imgui.h>

namespace FredEmmott::OpenXRLayers {
class APILayerStore;
class ReadWriteAPILayerStore;
struct LoaderData;

struct DPIChangeInfo {
  float mDPIScaling {};
  std::optional<ImVec2> mRecommendedSize;
};

// Platform-specific functions implemented in PlatformGUI_P*.cpp
class Platform {
 public:
  static Platform& Get();
  virtual ~Platform() = default;

  virtual void GUIMain(std::function<void()> drawFrame) = 0;

  virtual std::expected<LoaderData, std::string> GetLoaderData() = 0;
  virtual std::vector<std::filesystem::path> GetNewAPILayerJSONPaths() = 0;
  virtual std::optional<std::filesystem::path> GetExportFilePath() = 0;
  virtual std::vector<std::string> GetEnvironmentVariableNames() = 0;
  virtual float GetDPIScaling() = 0;

  // Use OS/environment equivalent to Explorer
  virtual void ShowFolderContainingFile(const std::filesystem::path&) = 0;

  Platform(const Platform&) = delete;
  Platform(Platform&&) = delete;
  Platform& operator=(const Platform&) = delete;
  Platform& operator=(Platform&&) = delete;

 protected:
  Platform() = default;
};
}// namespace FredEmmott::OpenXRLayers