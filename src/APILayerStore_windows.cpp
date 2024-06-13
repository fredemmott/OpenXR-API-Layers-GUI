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

namespace FredEmmott::OpenXRLayers {

static constexpr auto SubKey {
  L"SOFTWARE\\Khronos\\OpenXR\\1\\ApiLayers\\Implicit"};

class WindowsAPILayerStore final : public APILayerStore {
 public:
  WindowsAPILayerStore() = delete;
  WindowsAPILayerStore(auto displayName, REGSAM samFlags, HKEY rootKey)
    : mDisplayName(displayName) {
    if (
      RegOpenKeyExW(rootKey, SubKey, 0, KEY_ALL_ACCESS | samFlags, &mKey)
      != ERROR_SUCCESS) {
      mKey = {};
      return;
    }
    mEvent = CreateEvent(nullptr, false, false, nullptr);
    this->Poll();
  }

  std::string GetDisplayName() const noexcept override {
    return mDisplayName;
  }

  std::vector<APILayer> GetAPILayers() const noexcept override {
    if (!mKey) {
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
             mKey,
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
    return layers;
  }

  bool SetAPILayers(
    const std::vector<APILayer>& newLayers) const noexcept override {
    BackupAPILayers();

    const auto oldLayers = GetAPILayers();
    if (oldLayers == newLayers) {
      return false;
    }

    if (!mKey) {
      return false;
    }

    for (const auto& layer: oldLayers) {
      RegDeleteValueW(mKey, layer.mJSONPath.wstring().c_str());
    }

    for (const auto& layer: newLayers) {
      DWORD disabled = layer.IsEnabled() ? 0 : 1;
      RegSetValueExW(
        mKey,
        layer.mJSONPath.wstring().c_str(),
        NULL,
        REG_DWORD,
        reinterpret_cast<BYTE*>(&disabled),
        sizeof(disabled));
    }
    return true;
  }

  bool Poll() const noexcept override {
    const auto ret = WaitForSingleObject(mEvent, 0);
    RegNotifyChangeKeyValue(
      mKey, false, REG_NOTIFY_CHANGE_LAST_SET, mEvent, true);
    return ret;
  }

  virtual ~WindowsAPILayerStore() {
    RegCloseKey(mKey);
    CloseHandle(mEvent);
  }

 private:
  const std::string mDisplayName;

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
  static const WindowsAPILayerStore sHKLM64 {
    "Win64-HKLM", KEY_WOW64_64KEY, HKEY_LOCAL_MACHINE};
  static const WindowsAPILayerStore sHKCU64 {
    "Win64-HKCU", KEY_WOW64_64KEY, HKEY_CURRENT_USER};
  static const WindowsAPILayerStore sHKLM32 {
    "Win32-HKLM", KEY_WOW64_32KEY, HKEY_LOCAL_MACHINE};
  static const WindowsAPILayerStore sHKCU32 {
    "Win32-HKCU", KEY_WOW64_32KEY, HKEY_CURRENT_USER};
  static const APILayerStore* sStores[] {
    &sHKLM64,
    &sHKCU64,
    &sHKLM32,
    &sHKCU32,
  };
  return sStores;
};

}// namespace FredEmmott::OpenXRLayers