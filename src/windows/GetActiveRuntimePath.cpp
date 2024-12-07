// Copyright 2023 Fred Emmott <fred@fredemmott.com>

#include "../GetActiveRuntimePath.hpp"

#include <Windows.h>

#include <assert.h>

namespace FredEmmott::OpenXRLayers {

static LSTATUS GetActiveRuntimePath(void* data, DWORD* dataSize) {
  return RegGetValueW(
    HKEY_LOCAL_MACHINE,
    L"SOFTWARE\\Khronos\\OpenXR\\1",
    L"ActiveRuntime",
    RRF_RT_REG_SZ,
    nullptr,
    data,
    dataSize);
}

std::filesystem::path GetActiveRuntimePath() {
  DWORD dataSize {0};
  if (GetActiveRuntimePath(nullptr, &dataSize) != ERROR_SUCCESS) {
    return {};
  }
  if (dataSize == 0) {
    return {};
  }
  assert(dataSize % sizeof(wchar_t) == 0);

  std::wstring ret;
  ret.resize(dataSize / sizeof(WCHAR));
  GetActiveRuntimePath(ret.data(), &dataSize);
  while (ret.back() == L'\0') {
    ret.pop_back();
  }

  return ret;
}

}// namespace FredEmmott::OpenXRLayers
