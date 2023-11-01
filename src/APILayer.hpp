// Copyright 2023 Fred Emmott <fred@fredemmott.com>
// SPDX-License-Identifier: ISC

#pragma once

#include <filesystem>

namespace FredEmmott::OpenXRLayers {

struct APILayer {
  std::filesystem::path mPath {};
  bool mIsEnabled {};
};

}// namespace FredEmmott::OpenXRLayers
