// Copyright 2023 Fred Emmott <fred@fredemmott.com>
// SPDX-License-Identifier: ISC
#pragma once

#include <winrt/base.h>

#include <filesystem>
#include <mutex>
#include <string>

#include <ShlObj.h>

namespace FredEmmott::OpenXRLayers {

template <const GUID& knownFolderID>
auto GetKnownFolderPath() {
  static std::filesystem::path sPath;
  static std::once_flag sOnce;

  std::call_once(sOnce, [&path = sPath]() {
    wchar_t* buf {nullptr};
    try {
      winrt::check_hresult(
        SHGetKnownFolderPath(knownFolderID, KF_FLAG_DEFAULT, NULL, &buf));
    } catch (...) {
      CoTaskMemFree(buf);
      throw;
    }

    path = {std::wstring_view {buf}};
    if (std::filesystem::exists(path)) {
      path = std::filesystem::canonical(path);
    }

    CoTaskMemFree(buf);
  });

  return sPath;
}

}// namespace FredEmmott::OpenXRLayers