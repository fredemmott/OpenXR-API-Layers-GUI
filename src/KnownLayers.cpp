// Copyright 2023 Fred Emmott <fred@fredemmott.com>
// SPDX-License-Identifier: ISC

#include "KnownLayers.hpp"

namespace FredEmmott::OpenXRLayers {

// These are currently required to be human-readable, but that may change in the
// future.
namespace Features {
const char* Overlay = "an overlay";
}// namespace Features

std::unordered_map<std::string, KnownLayer> GetKnownLayers() {
  static std::unordered_map<std::string, KnownLayer> ret;
  if (!ret.empty()) {
    return ret;
  }

  const KnownLayer list[] {
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
        "XR_EXT_eye_gaze_interaction",
      },
      .mBelow = {
        "XR_VARJO_foveated_rendering",
      },
      .mProvides = {
        Features::Overlay,
      },
      .mConflictsPerApp = {
        "XR_APILAYER_MBUCCHIA_varjo_foveated",
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
      .mName = "XR_APILAYER_MBUCCHIA_vulkan_d3d12_interop",
      .mAbove = {
        // Incompatible with Vulkan:
        "XR_APILAYER_MBUCCHIA_toolkit",
        "XR_APILAYER_NOVENDOR_OBSMirror",
      },
    },
    {
      .mName = "XR_APILAYER_NOVENDOR_OBSMirror",
      .mBelow = {
        Features::Overlay,
        "XR_VARJO_foveated_rendering",
      },
    },
    {
      .mName = "XR_APILAYER_NOVENDOR_XRNeckSafer",
      .mAbove = {
        // Unknown incompatibility issue:
        "XR_APILAYER_FREDEMMOTT_HandTrackedCockpitClicking",
        // - https://gitlab.com/NobiWan/xrnecksafer/-/issues/15
        // - https://gitlab.com/NobiWan/xrnecksafer/-/issues/16
        // - Other developers have mentioned thread safety issues in XRNS that can cause crashes; I've not confirmed these
        "XR_APILAYER_FREDEMMOTT_OpenKneeboard",
      },
    },
  };

  for (auto&& layer: list) {
    ret.emplace(layer.mName, layer);
  }
  return ret;
}

}// namespace FredEmmott::OpenXRLayers