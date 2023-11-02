// Copyright 2023 Fred Emmott <fred@fredemmott.com>
// SPDX-License-Identifier: ISC

#include "KnownLayers.hpp"

namespace FredEmmott::OpenXRLayers {

// These are currently required to be human-readable, but that may change in the
// future.
namespace Features {
const char* Overlay = "an overlay";
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
      .mName = "XR_APILAYER_MBUCCHIA_quad_views_foveated",
      .mAbove = {
        "XR_EXT_eye_gaze_interaction",
      },
    },
    {
      .mName = "XR_APILAYER_MBUCCHIA_toolkit",
      .mAbove = {
        "XR_EXT_hand_tracking",
      },
      .mProvides = {
        Features::Overlay,
      },
    },
    {
      .mName = "XR_APILAYER_NOVENDOR_motion_compensation",
      .mAbove = {
        // Unknown incompatibilty issue:
        "XR_APILAYER_FREDEMMOTT_HandTrackedCockpitClicking",
      },
    },
    {
      .mName = "XR_APILAYER_NOVENDOR_OBSMirror",
      .mBelow = {
        Features::Overlay,
      },
    },
    {
      .mName = "XR_APILAYER_NOVENDOR_XRNeckSafer",
      .mAbove = {
        // Unknown incompatibility issue:
        "XR_APILAYER_FREDEMMOTT_HandTrackedCockpitClicking",
      },
    },
  };

  for (const auto& layer: list) {
    ret.emplace(layer.mName, std::move(layer));
  }
  return ret;
}

}// namespace FredEmmott::OpenXRLayers