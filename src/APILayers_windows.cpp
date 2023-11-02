// Copyright 2023 Fred Emmott <fred@fredemmott.com>
// SPDX-License-Identifier: ISC

#include <Windows.h>

#include <fmt/chrono.h>
#include <fmt/core.h>

#include <chrono>
#include <filesystem>
#include <fstream>

#include <ShlObj.h>

#include "APILayers.hpp"
#include "Config.hpp"
#include "Config_windows.hpp"

namespace FredEmmott::OpenXRLayers {

constexpr auto SUBKEY {L"SOFTWARE\\Khronos\\OpenXR\\1\\ApiLayers\\Implicit"};

std::vector<APILayer> GetAPILayers() {
  HKEY key {NULL};
  if (RegOpenKeyW(Config::APILAYER_HKEY_ROOT, SUBKEY, &key) != ERROR_SUCCESS) {
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
    if (dataType == REG_DWORD) {
      layers.push_back({
        .mJSONPath = {std::wstring_view {nameBuffer, nameSize}},
        .mIsEnabled = !disabled,
      });
    }

    nameSize = maxNameSize;
    disabledSize = sizeof(disabled);
  }

  RegCloseKey(key);
  return layers;
}

static void BackupAPILayers() {
  static bool haveBackup {false};
  if (haveBackup) {
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
    f << (layer.mIsEnabled ? "0" : "1") << "\t" << layer.mJSONPath.string()
      << "\n";
  }

  haveBackup = true;
}

bool SetAPILayers(const std::vector<APILayer>& newLayers) {
  BackupAPILayers();

  const auto oldLayers = GetAPILayers();
  if (oldLayers == newLayers) {
    return false;
  }

  HKEY key {NULL};
  if (RegOpenKeyW(Config::APILAYER_HKEY_ROOT, SUBKEY, &key) != ERROR_SUCCESS) {
    return false;
  }

  for (const auto& layer: oldLayers) {
    RegDeleteValueW(key, layer.mJSONPath.wstring().c_str());
  }

  for (const auto& layer: newLayers) {
    DWORD disabled = layer.mIsEnabled ? 0 : 1;
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

}// namespace FredEmmott::OpenXRLayers