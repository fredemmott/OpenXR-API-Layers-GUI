// Copyright 2023 Fred Emmott <fred@fredemmott.com>
// SPDX-License-Identifier: ISC
#pragma once

#include <winrt/base.h>

namespace FredEmmott::OpenXRLayers {

class AutoUpdateProcess {
 public:
  AutoUpdateProcess() = default;
  AutoUpdateProcess(winrt::handle job, winrt::handle jobCompletion);

  void ActivateWindowIfVisible();

 private:
  [[nodiscard]] bool IsRunning() const;

  winrt::handle mJob;
  winrt::handle mJobCompletion;

  bool mHaveActivatedWindow = false;
};

[[nodiscard]] AutoUpdateProcess CheckForUpdates();

}// namespace FredEmmott::OpenXRLayers