// Copyright 2023 Fred Emmott <fred@fredemmott.com>
// SPDX-License-Identifier: ISC

#include "KnownLayers.hpp"

namespace FredEmmott::OpenXRLayers {

std::unordered_map<std::string, KnownLayer> GetKnownLayers() {
  static std::unordered_map<std::string, KnownLayer> ret;
  if (!ret.empty()) {
    return ret;
  }

  const std::vector<KnownLayer> list {
    {
      .mName = "XR_APILAYER_FREDEMMOTT_HandTrackedCockpitClicking",
      .mConsumes = {
        "XR_EXT_hand_tracking",
      },
    },
  };

  for (const auto& layer: list) {
    ret.emplace(layer.mName, std::move(layer));
  }
  return ret;
}

}// namespace FredEmmott::OpenXRLayers