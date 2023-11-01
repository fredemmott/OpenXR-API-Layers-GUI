// Copyright 2023 Fred Emmott <fred@fredemmott.com>
// SPDX-License-Identifier: ISC

#include <Windows.h>

#include "APILayers.hpp"
#include "Config.hpp"
#include "Config_windows.hpp"

namespace FredEmmott::OpenXRLayers {

std::vector<APILayer> GetAPILayers() {
  HKEY key {NULL};
  if (
    RegOpenKeyW(
      Config::APILAYER_HKEY_ROOT,
      L"SOFTWARE\\Khronos\\OpenXR\\1\\ApiLayers\\Implicit",
      &key)
    != ERROR_SUCCESS) {
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
        .mPath = {std::wstring_view {nameBuffer, nameSize}},
        .mIsEnabled = !disabled,
      });
    }

    nameSize = maxNameSize;
    disabledSize = sizeof(disabled);
  }

  RegCloseKey(key);
  return layers;
}

void SetAPILayers(const std::vector<APILayer>&) {
}

}// namespace FredEmmott::OpenXRLayers