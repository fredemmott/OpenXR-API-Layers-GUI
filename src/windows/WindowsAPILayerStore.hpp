// Copyright 2023 Fred Emmott <fred@fredemmott.com>
// SPDX-License-Identifier: ISC

#include <Windows.h>

#include <winrt/base.h>

#include <wil/registry.h>

#include "APILayerStore.hpp"

namespace FredEmmott::OpenXRLayers {

class WindowsAPILayerStore : public virtual APILayerStore {
 public:
  enum class RegistryBitness {
    Wow64_64 = sizeof(uint64_t),
    Wow64_32 = sizeof(uint32_t),
  };

  WindowsAPILayerStore() = delete;
  ~WindowsAPILayerStore() override;

  std::string GetDisplayName() const noexcept override;

  std::vector<APILayer> GetAPILayers() const noexcept override;

  bool Poll() const noexcept override;

  bool IsForCurrentArchitecture() const noexcept override {
    return std::to_underlying(mRegistryBitness) == sizeof(void*);
  }

  RegistryBitness GetRegistryBitness() const noexcept {
    return mRegistryBitness;
  }

  HKEY GetRootKey() const noexcept {
    return mRootKey;
  }

 protected:
  WindowsAPILayerStore(
    std::string_view displayName,
    RegistryBitness bitness,
    HKEY rootKey,
    REGSAM desiredAccess);

  wil::unique_hkey mKey {};
  wil::unique_event mEvent {};

 private:
  const std::string mDisplayName;
  const RegistryBitness mRegistryBitness;
  const HKEY mRootKey;
};

}// namespace FredEmmott::OpenXRLayers