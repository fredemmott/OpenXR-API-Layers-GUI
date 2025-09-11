// Copyright 2023 Fred Emmott <fred@fredemmott.com>
// SPDX-License-Identifier: ISC

#include "WindowsAPILayerStore.hpp"

#include <Windows.h>

#include <fmt/chrono.h>
#include <fmt/core.h>

#include <chrono>
#include <filesystem>
#include <fstream>

#include <ShlObj.h>

#include "APILayerStore.hpp"
#include "Config.hpp"
#include "windows/GetKnownFolderPath.hpp"

namespace FredEmmott::OpenXRLayers {

static constexpr auto SubKey {
  L"SOFTWARE\\Khronos\\OpenXR\\1\\ApiLayers\\Implicit"};

WindowsAPILayerStore::WindowsAPILayerStore(
  std::string_view displayName,
  RegistryBitness bitness,
  HKEY rootKey,
  REGSAM desiredAccess)
  : mDisplayName(displayName),
    mRegistryBitness(bitness),
    mRootKey(rootKey) {
  REGSAM samFlags {desiredAccess};
  switch (bitness) {
    case RegistryBitness::Wow64_64:
      samFlags |= KEY_WOW64_64KEY;
      break;
    case RegistryBitness::Wow64_32:
      samFlags |= KEY_WOW64_32KEY;
      break;
  }
  if (
    RegCreateKeyExW(
      rootKey,
      SubKey,
      0,
      nullptr,
      REG_OPTION_NON_VOLATILE,
      samFlags,
      nullptr,
      mKey.put(),
      nullptr)
    != ERROR_SUCCESS) {
#ifndef NDEBUG
    __debugbreak();
#endif
    mKey = {};
    return;
  }
  mEvent.reset(CreateEvent(nullptr, false, false, nullptr));
  this->Poll();
}

WindowsAPILayerStore::~WindowsAPILayerStore() = default;

std::string WindowsAPILayerStore::GetDisplayName() const noexcept {
  return mDisplayName;
}

std::vector<APILayer> WindowsAPILayerStore::GetAPILayers() const noexcept {
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
  bool moreItems = true;
  while (moreItems) {
    using Value = APILayer::Value;
    const auto result = RegEnumValueW(
      mKey.get(),
      index++,
      nameBuffer,
      &nameSize,
      nullptr,
      &dataType,
      reinterpret_cast<LPBYTE>(&disabled),
      &disabledSize);
    switch (result) {
      case ERROR_SUCCESS:
        break;
      case ERROR_NO_MORE_ITEMS:
        moreItems = false;
        continue;
      case ERROR_MORE_DATA:
        // Bigger than a DWORD means not a DWORD< so handled by dataType check
        break;
      default:
#ifndef NDEBUG
        __debugbreak();
#endif
        break;
    }

    if (dataType == REG_DWORD) {
      layers.emplace_back(
        this,
        std::wstring_view {nameBuffer, nameSize},
        disabled ? Value::Disabled : Value::Enabled);
    } else {
      layers.emplace_back(
        this, std::wstring_view {nameBuffer, nameSize}, Value::Win32_NotDWORD);
    }

    nameSize = maxNameSize;
    disabledSize = sizeof(disabled);
  }
  return layers;
}

bool WindowsAPILayerStore::Poll() const noexcept {
  const auto result = WaitForSingleObject(mEvent.get(), 0);
  RegNotifyChangeKeyValue(
    mKey.get(), false, REG_NOTIFY_CHANGE_LAST_SET, mEvent.get(), true);
  return (result == WAIT_OBJECT_0);
}

class ReadOnlyWindowsAPILayerStore final : public WindowsAPILayerStore {
 public:
  ReadOnlyWindowsAPILayerStore(
    std::string_view displayName,
    RegistryBitness bitness,
    HKEY rootKey)
    : WindowsAPILayerStore(displayName, bitness, rootKey, KEY_READ) {}
};

class ReadWriteWindowsAPILayerStore final : public WindowsAPILayerStore,
                                            public ReadWriteAPILayerStore {
 public:
  ReadWriteWindowsAPILayerStore(
    std::string_view displayName,
    RegistryBitness bitness,
    HKEY rootKey)
    : WindowsAPILayerStore(
        displayName,
        bitness,
        rootKey,
        KEY_READ | KEY_WRITE) {}

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
      RegDeleteValueW(mKey.get(), layer.mManifestPath.wstring().c_str());
    }

    for (const auto& layer: newLayers) {
      DWORD disabled = layer.IsEnabled() ? 0 : 1;
      RegSetValueExW(
        mKey.get(),
        layer.mManifestPath.wstring().c_str(),
        NULL,
        REG_DWORD,
        reinterpret_cast<BYTE*>(&disabled),
        sizeof(disabled));
    }
    return true;
  }

 private:
  mutable bool mHaveBackup {false};

  void BackupAPILayers() const {
    if (mHaveBackup) {
      return;
    }

    const auto backupFolder = GetKnownFolderPath<FOLDERID_LocalAppData>()
      / "OpenXR API Layers GUI" / "Backups";
    if (!std::filesystem::is_directory(backupFolder)) {
      std::filesystem::create_directories(backupFolder);
    }

    const auto path = backupFolder
      / fmt::format("{:%F-%H-%M-%S}-{}.tsv",
                    std::chrono::time_point_cast<std::chrono::seconds>(
                      std::chrono::system_clock::now()),
                    this->GetDisplayName());

    std::ofstream f(path);
    for (const auto& layer: GetAPILayers()) {
      f << (layer.IsEnabled() ? "0" : "1") << "\t"
        << layer.mManifestPath.string() << "\n";
    }

    mHaveBackup = true;
  }
};

template <class TInterface, class TConcrete>
std::span<const TInterface*> GetStaticStores() noexcept {
  using RB = WindowsAPILayerStore::RegistryBitness;
  static const TConcrete sHKLM64 {
    "Win64-HKLM", RB::Wow64_64, HKEY_LOCAL_MACHINE};
  static const TConcrete sHKCU64 {
    "Win64-HKCU", RB::Wow64_64, HKEY_CURRENT_USER};
  static const TConcrete sHKLM32 {
    "Win32-HKLM", RB::Wow64_32, HKEY_LOCAL_MACHINE};
  static const TConcrete sHKCU32 {
    "Win32-HKCU", RB::Wow64_32, HKEY_CURRENT_USER};
  static const TInterface* sStores[] {
    &sHKLM64,
    &sHKCU64,
    &sHKLM32,
    &sHKCU32,
  };
  return sStores;
}

std::span<const APILayerStore*> APILayerStore::Get() noexcept {
  return GetStaticStores<APILayerStore, ReadOnlyWindowsAPILayerStore>();
};

std::span<const ReadWriteAPILayerStore*>
ReadWriteAPILayerStore::Get() noexcept {
  return GetStaticStores<
    ReadWriteAPILayerStore,
    ReadWriteWindowsAPILayerStore>();
};

}// namespace FredEmmott::OpenXRLayers