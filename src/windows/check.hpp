// Copyright 2023 Fred Emmott <fred@fredemmott.com>
// SPDX-License-Identifier: ISC
#pragma once

#include <Windows.h>

#include <source_location>
#include <system_error>

namespace FredEmmott::OpenXRLayers {

inline void CheckHRESULT(const HRESULT result) {
  if (SUCCEEDED(result)) {
    return;
  }
  throw std::system_error(result, std::system_category());
}

inline void CheckBOOL(const BOOL result) {
  if (result) {
    return;
  }
  throw std::system_error(
    HRESULT_FROM_WIN32(GetLastError()), std::system_category());
}

}// namespace FredEmmott::OpenXRLayers