// Copyright 2023 Fred Emmott <fred@fredemmott.com>
// SPDX-License-Identifier: ISC
#pragma once

#include <wil/resource.h>

namespace FredEmmott::OpenXRLayers {

class AutoUpdateProcess {
 public:
  AutoUpdateProcess() = default;
  AutoUpdateProcess(wil::unique_handle job, wil::unique_handle jobCompletion);

  void ActivateWindowIfVisible();

 private:
  [[nodiscard]] bool IsRunning() const;

  wil::unique_handle mJob;
  wil::unique_handle mJobCompletion;

  bool mHaveActivatedWindow = false;
};

[[nodiscard]] AutoUpdateProcess CheckForUpdates();

}// namespace FredEmmott::OpenXRLayers