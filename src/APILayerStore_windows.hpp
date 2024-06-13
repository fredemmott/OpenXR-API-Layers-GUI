// Copyright 2023 Fred Emmott <fred@fredemmott.com>
// SPDX-License-Identifier: ISC

#include <Windows.h>

#include "APILayerStore.hpp"

namespace FredEmmott::OpenXRLayers {

class WindowsAPILayerStore : public virtual APILayerStore {
 public:
  enum class RegistryBitness {
    Wow64_64,
    Wow64_32,
  };

  WindowsAPILayerStore() = delete;
  virtual ~WindowsAPILayerStore();

  std::string GetDisplayName() const noexcept override;

  std::vector<APILayer> GetAPILayers() const noexcept override;

  bool Poll() const noexcept override;

  inline RegistryBitness GetRegistryBitness() const noexcept {
    return mRegistryBitness;
  }

  inline HKEY GetRootKey() const noexcept {
    return mRootKey;
  }

 protected:
  WindowsAPILayerStore(
    std::string_view displayName,
    RegistryBitness bitness,
    HKEY rootKey,
    REGSAM desiredAccess);

  HKEY mKey {};
  HANDLE mEvent {};

 private:
  const std::string mDisplayName;
  const RegistryBitness mRegistryBitness;
  const HKEY mRootKey;
};

}// namespace FredEmmott::OpenXRLayers