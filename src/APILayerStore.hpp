// Copyright 2023 Fred Emmott <fred@fredemmott.com>
// SPDX-License-Identifier: ISC

#pragma once
#include <boost/signals2.hpp>

#include <span>
#include <vector>

#include "APILayer.hpp"
#include "Architectures.hpp"
#include "Platform.hpp"

namespace FredEmmott::OpenXRLayers {

class APILayerStore {
 public:
  virtual ~APILayerStore() = default;

  virtual APILayer::Kind GetKind() const noexcept = 0;

  // e.g. "Win64-HKLM"
  virtual std::string GetDisplayName() const noexcept = 0;
  virtual std::vector<APILayer> GetAPILayers() const noexcept = 0;
  virtual Architectures GetArchitectures() const noexcept = 0;
  // e.g. if we're a 64-bit build, we won't see 32-bit layers
  [[nodiscard]]
  bool IsForCurrentArchitecture() const noexcept {
    return GetArchitectures().contains(Platform::GetBuildArchitecture());
  }

  boost::signals2::scoped_connection OnChange(
    std::function<void()> callback) noexcept {
    return mOnChangeSignal.connect(std::move(callback));
  }

  static std::span<APILayerStore*> Get() noexcept;

 protected:
  void NotifyChange() {
    mOnChangeSignal();
  }

 private:
  boost::signals2::signal<void()> mOnChangeSignal;
};

class ReadWriteAPILayerStore : public virtual APILayerStore {
 public:
  virtual bool SetAPILayers(const std::vector<APILayer>&) const noexcept = 0;
};

}// namespace FredEmmott::OpenXRLayers
