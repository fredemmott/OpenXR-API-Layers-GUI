// Copyright 2025 Fred Emmott <fred@fredemmott.com>
// SPDX-License-Identifier: MIT
#pragma once

#include <wil/com.h>
#include <wil/resource.h>

#include <mutex>

#include <d3d11.h>
#include <dxgi1_2.h>

#include "CheckForUpdates.hpp"
#include "Config.hpp"
#include "LoaderData.hpp"
#include "Platform.hpp"

namespace FredEmmott::OpenXRLayers {
class WindowsPlatform final : public Platform {
 public:
  void GUIMain(std::function<void()> drawFrame) override;

  std::optional<std::filesystem::path> GetExportFilePath() override;
  std::vector<std::filesystem::path> GetNewAPILayerJSONPaths() override;
  std::expected<LoaderData, LoaderData::Error> GetLoaderData() override;

  float GetDPIScaling() override {
    return mDPIScaling;
  }
  void ShowFolderContainingFile(const std::filesystem::path& path) override;
  std::unordered_set<std::string> GetEnvironmentVariableNames() override;

  std::vector<AvailableRuntime> GetAvailable32BitRuntimes() override;
  std::vector<AvailableRuntime> GetAvailable64BitRuntimes() override;
  std::filesystem::file_time_type GetFileChangeTime(
    const std::filesystem::path& path) override;

  std::expected<APILayerSignature, APILayerSignature::Error>
  GetAPILayerSignature(const std::filesystem::path&) override;

  std::vector<std::string> GetEnabledExplicitAPILayers() override;
  std::optional<std::vector<std::filesystem::path>> GetOverridePaths()
    const override;

  Architectures GetArchitectures() const override;
  Architectures GetSharedLibraryArchitectures(
    const std::filesystem::path&) const override;

 protected:
  std::filesystem::path Get32BitRuntimePath() override;
  std::filesystem::path Get64BitRuntimePath() override;

 private:
  wil::unique_hwnd mWindowHandle {};
  float mDPIScaling {};
  std::optional<DPIChangeInfo> mDPIChangeInfo;
  AutoUpdateProcess mUpdater {CheckForUpdates()};

  wil::com_ptr<ID3D11Device> mD3DDevice;
  wil::com_ptr<ID3D11DeviceContext> mD3DContext;
  wil::com_ptr<IDXGISwapChain1> mSwapChain;
  wil::com_ptr<ID3D11Texture2D> mBackBuffer;
  wil::com_ptr<ID3D11RenderTargetView> mRenderTargetView;
  bool mWindowClosed = false;

  size_t mFrameCounter = 0;
  ImVec2 mWindowSize {
    Config::MINIMUM_WINDOW_WIDTH,
    Config::MINIMUM_WINDOW_HEIGHT,
  };
  std::mutex mMutex;
  std::optional<std::expected<LoaderData, LoaderData::Error>> mLoaderData;

  HWND CreateAppWindow();
  void InitializeFonts(ImGuiIO* io);
  void InitializeDirect3D();

  void BeforeFrame();
  void AfterFrame();

  void Initialize();
  void MainLoop(const std::function<void()>& drawFrame);
  void Shutdown();

  static std::expected<LoaderData, LoaderData::Error>
  GetLoaderDataWithoutCache();
  static LRESULT WindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

  LRESULT
  InstanceWindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
};
}// namespace FredEmmott::OpenXRLayers
