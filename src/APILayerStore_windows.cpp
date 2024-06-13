// Copyright 2023 Fred Emmott <fred@fredemmott.com>
// SPDX-License-Identifier: ISC

#include <Windows.h>

#include <fmt/chrono.h>
#include <fmt/core.h>

#include <chrono>
#include <filesystem>
#include <fstream>

#include <ShlObj.h>

#include "APILayerStore.hpp"
#include "Config.hpp"
#include "Config_windows.hpp"

namespace FredEmmott::OpenXRLayers {

static constexpr auto SUBKEY {
  L"SOFTWARE\\Khronos\\OpenXR\\1\\ApiLayers\\Implicit"};

class WindowsAPILayerStore final : public APILayerStore {
 public:
  std::string GetDisplayName() const noexcept override {
    return std::string {Config::BUILD_TARGET_ID};
  }

  std::vector<APILayer> GetAPILayers() const noexcept override {
    HKEY key {NULL};
    if (
      RegOpenKeyW(Config::APILAYER_HKEY_ROOT, SUBKEY, &key) != ERROR_SUCCESS) {
      return {};
    }

    // https://docs.microsoft.com/en-us/windows/win32/sysinfo/registry-element-size-limits
    constexpr DWORD maxNameSize {16383};
    wchar_t nameBuffer[maxNameSize];
    DWORD nameSize {maxNameSize};
    DWORD dataType {};
    DWORD disabled {1};
    DWORD disabledSize {sizeof(disabled)};

    DWORD index = 0;

    std::vector<APILayer> layers;
    while (RegEnumValueW(
             key,
             index++,
             nameBuffer,
             &nameSize,
             nullptr,
             &dataType,
             reinterpret_cast<LPBYTE>(&disabled),
             &disabledSize)
           == ERROR_SUCCESS) {
      using Value = APILayer::Value;
      if (dataType == REG_DWORD) {
        layers.push_back({
          .mJSONPath = {std::wstring_view {nameBuffer, nameSize}},
          .mValue = disabled ? Value::Disabled : Value::Enabled,
        });
      } else {
        layers.push_back({
          .mJSONPath = {std::wstring_view {nameBuffer, nameSize}},
          .mValue = Value::Win32_NotDWORD,
        });
      }

      nameSize = maxNameSize;
      disabledSize = sizeof(disabled);
    }

    RegCloseKey(key);
    return layers;
  }

  bool SetAPILayers(
    const std::vector<APILayer>& newLayers) const noexcept override {
    BackupAPILayers();

    const auto oldLayers = GetAPILayers();
    if (oldLayers == newLayers) {
      return false;
    }

    HKEY key {NULL};
    if (
      RegOpenKeyW(Config::APILAYER_HKEY_ROOT, SUBKEY, &key) != ERROR_SUCCESS) {
      return false;
    }

    for (const auto& layer: oldLayers) {
      RegDeleteValueW(key, layer.mJSONPath.wstring().c_str());
    }

    for (const auto& layer: newLayers) {
      DWORD disabled = layer.IsEnabled() ? 0 : 1;
      RegSetValueExW(
        key,
        layer.mJSONPath.wstring().c_str(),
        NULL,
        REG_DWORD,
        reinterpret_cast<BYTE*>(&disabled),
        sizeof(disabled));
    }
    RegCloseKey(key);

    return true;
  }

  bool Poll() const noexcept override {
    const auto ret = WaitForSingleObject(mEvent, 0);
    RegNotifyChangeKeyValue(
      mKey, false, REG_NOTIFY_CHANGE_LAST_SET, mEvent, true);
    return ret;
  }

  WindowsAPILayerStore() {
    RegOpenKeyW(Config::APILAYER_HKEY_ROOT, SUBKEY, &mKey);
    mEvent = CreateEvent(nullptr, false, false, nullptr);
    this->Poll();
  }

  virtual ~WindowsAPILayerStore() {
    RegCloseKey(mKey);
    CloseHandle(mEvent);
  }

 private:
  HKEY mKey {};
  HANDLE mEvent {};

  mutable bool mHaveBackup = false;

  void BackupAPILayers() const {
    if (mHaveBackup) {
      return;
    }

    wchar_t* folderPath {nullptr};
    if (
      SHGetKnownFolderPath(
        FOLDERID_LocalAppData, KF_FLAG_DEFAULT, NULL, &folderPath)
      != S_OK) {
      return;
    }

    if (!folderPath) {
      return;
    }

    const auto backupFolder
      = std::filesystem::path(folderPath) / "OpenXR API Layers GUI" / "Backups";
    if (!std::filesystem::is_directory(backupFolder)) {
      std::filesystem::create_directories(backupFolder);
    }

    const auto path = backupFolder
      / fmt::format("{:%F-%H-%M-%S}-{}.tsv",
                    std::chrono::time_point_cast<std::chrono::seconds>(
                      std::chrono::system_clock::now()),
                    Config::BUILD_TARGET_ID);

    std::ofstream f(path);
    for (const auto& layer: GetAPILayers()) {
      f << (layer.IsEnabled() ? "0" : "1") << "\t" << layer.mJSONPath.string()
        << "\n";
    }

    mHaveBackup = true;
  }
};

std::span<const APILayerStore*> APILayerStore::Get() noexcept {
  static const WindowsAPILayerStore sInstance;
  static const APILayerStore* sStores[] {&sInstance};
  return sStores;
};

}// namespace FredEmmott::OpenXRLayers