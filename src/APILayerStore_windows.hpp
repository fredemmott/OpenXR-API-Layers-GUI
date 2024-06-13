// Copyright 2023 Fred Emmott <fred@fredemmott.com>
// SPDX-License-Identifier: ISC

#include <Windows.h>

#include "APILayerStore.hpp"

namespace FredEmmott::OpenXRLayers {

class WindowsAPILayerStore final : public APILayerStore {
 public:
  enum class RegistryBitness {
    Wow64_64,
    Wow64_32,
  };

  WindowsAPILayerStore() = delete;
  WindowsAPILayerStore(
    std::string_view displayName,
    RegistryBitness bitness,
    HKEY rootKey);
  virtual ~WindowsAPILayerStore();

  std::string GetDisplayName() const noexcept override;

  std::vector<APILayer> GetAPILayers() const noexcept override;

  bool SetAPILayers(
    const std::vector<APILayer>& newLayers) const noexcept override;

  bool Poll() const noexcept override;

  inline RegistryBitness GetRegistryBitness() const noexcept {
    return mRegistryBitness;
  }

  inline HKEY GetRootKey() const noexcept {
    return mRootKey;
  }

 private:
  const std::string mDisplayName;
  const RegistryBitness mRegistryBitness;
  const HKEY mRootKey;

  HKEY mKey {};
  HANDLE mEvent {};

  mutable bool mHaveBackup {false};

  void BackupAPILayers() const;
};

}// namespace FredEmmott::OpenXRLayers