// Copyright 2023 Fred Emmott <fred@fredemmott.com>
// SPDX-License-Identifier: ISC
#pragma once

#include <wil/resource.h>

#include <filesystem>
#include <mutex>

#include <ShlObj.h>

#include "windows/check.hpp"

namespace FredEmmott::OpenXRLayers {

template <const GUID& knownFolderID>
auto GetKnownFolderPath() {
  static std::filesystem::path sPath;
  static std::once_flag sOnce;

  std::call_once(sOnce, [&path = sPath]() {
    wil::unique_cotaskmem_string buf;
    CheckHRESULT(
      SHGetKnownFolderPath(knownFolderID, KF_FLAG_DEFAULT, NULL, buf.put()));

    path = {std::wstring_view {buf.get()}};
    if (std::filesystem::exists(path)) {
      path = std::filesystem::canonical(path);
    }
  });

  return sPath;
}

}// namespace FredEmmott::OpenXRLayers