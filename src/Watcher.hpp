// Copyright 2023 Fred Emmott <fred@fredemmott.com>
// SPDX-License-Identifier: ISC

#pragma once

namespace FredEmmott::OpenXRLayers {

// Efficiently watch for external changes to the OpenXR layers
class Watcher {
 public:
  virtual ~Watcher() = default;
  virtual bool Poll() = 0;

  static Watcher& Get();
};

}// namespace FredEmmott::OpenXRLayers