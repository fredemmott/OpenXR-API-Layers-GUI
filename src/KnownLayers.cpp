// Copyright 2023 Fred Emmott <fred@fredemmott.com>
// SPDX-License-Identifier: ISC

#include "KnownLayers.hpp"

namespace FredEmmott::OpenXRLayers {

namespace Features {
const std::string Overlay = "!overlay";
}

std::unordered_map<std::string, KnownLayer> GetKnownLayers() {
  static std::unordered_map<std::string, KnownLayer> ret;
  if (!ret.empty()) {
    return ret;
  }

  const std::vector<KnownLayer> list {
    {
      .mName = "XR_APILAYER_FREDEMMOTT_HandTrackedCockpitClicking",
      .mAbove = {
        "XR_EXT_hand_tracking",
      },
    },
    {
      .mName = "XR_APILAYER_FREDEMMOTT_OpenKneeboard",
      .mProvides = {
        Features::Overlay,
      },
    },
    {
      .mName = "XR_APILAYER_NOVENDOR_OBSMirror",
      .mBelow = {
        Features::Overlay,
      },
    },
  };

  for (const auto& layer: list) {
    ret.emplace(layer.mName, std::move(layer));
  }
  return ret;
}

}// namespace FredEmmott::OpenXRLayers