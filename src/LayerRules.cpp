// Copyright 2023 Fred Emmott <fred@fredemmott.com>
// SPDX-License-Identifier: ISC

#include "LayerRules.hpp"

namespace FredEmmott::OpenXRLayers {

inline namespace LayerIDs {
#define DEFINE_LAYER_ID(x) static constexpr LayerID x {#x};
DEFINE_LAYER_ID(XR_APILAYER_FREDEMMOTT_HandTrackedCockpitClicking)
DEFINE_LAYER_ID(XR_APILAYER_FREDEMMOTT_OpenKneeboard)
DEFINE_LAYER_ID(XR_APILAYER_MBUCCHIA_quad_views_foveated)
DEFINE_LAYER_ID(XR_APILAYER_MBUCCHIA_toolkit)
DEFINE_LAYER_ID(XR_APILAYER_MBUCCHIA_varjo_foveated)
DEFINE_LAYER_ID(XR_APILAYER_MBUCCHIA_vulkan_d3d12_interop)
DEFINE_LAYER_ID(XR_APILAYER_NOVENDOR_motion_compensation)
DEFINE_LAYER_ID(XR_APILAYER_NOVENDOR_OBSMirror)
DEFINE_LAYER_ID(XR_APILAYER_NOVENDOR_XRNeckSafer)
DEFINE_LAYER_ID(XR_APILAYER_app_racelab_Overlay)
#undef DEFINE_LAYER_ID
}// namespace LayerIDs

inline namespace ExtensionIDs {
#define DEFINE_EXTENSION_ID(x) static constexpr ExtensionID x {#x};
DEFINE_EXTENSION_ID(XR_EXT_eye_gaze_interaction)
DEFINE_EXTENSION_ID(XR_EXT_hand_tracking)
DEFINE_EXTENSION_ID(XR_VARJO_foveated_rendering)
#undef DEFINE_EXTENSION_ID
}// namespace ExtensionIDs

namespace Facets {
#define DEFINE_FACET(name, description) \
  static constexpr Facet name {"#" #name, description};
DEFINE_FACET(CompositionLayers, "provides an overlay")
DEFINE_FACET(TransformsPoses, "modifies poses")
DEFINE_FACET(UsesGameWorldPoses, "uses poses")
#undef DEFINE_FACET

}// namespace Facets

namespace {
struct Literals {
  Literals() = delete;
  template <class... Args>
  explicit Literals(Args&&... args) noexcept {
    mRet = {{Facet {args}, {}}...};
  }

  operator FacetMap() const {// NOLINT(*-explicit-constructor)
    return mRet;
  }

 private:
  FacetMap mRet;
};
}// namespace

std::vector<LayerRules> GetLayerRules() {
  return {
    {
      .mID = Facets::TransformsPoses,
      .mBelow = Literals {
        Facets::UsesGameWorldPoses,
      },
    },
    {
      .mID = XR_APILAYER_FREDEMMOTT_HandTrackedCockpitClicking,
      .mAbove = Literals {
        XR_EXT_hand_tracking,
      },
    },
    {
      .mID = XR_APILAYER_FREDEMMOTT_OpenKneeboard,
      .mFacets = Literals {
        Facets::CompositionLayers,
        Facets::UsesGameWorldPoses,
      },
    },
    {
      .mID = XR_APILAYER_app_racelab_Overlay,
      .mFacets = Literals {
        Facets::CompositionLayers,
        Facets::UsesGameWorldPoses,
      },
    },
    {
      .mID = XR_APILAYER_MBUCCHIA_quad_views_foveated,
      .mAbove = Literals {
        XR_EXT_eye_gaze_interaction,
      },
    },
    {
      .mID = XR_APILAYER_MBUCCHIA_toolkit,
      .mAbove = Literals {
        XR_EXT_eye_gaze_interaction,
        XR_EXT_hand_tracking,
      },
      .mBelow = Literals {
        XR_VARJO_foveated_rendering,
      },
      .mFacets = Literals {
        Facets::CompositionLayers,
      },
      .mConflictsPerApp = Literals {
        XR_APILAYER_MBUCCHIA_varjo_foveated,
      },
    },
    {
      .mID = XR_APILAYER_NOVENDOR_motion_compensation,
      .mAbove = Literals {
        // Unknown incompatibility issue:
        XR_APILAYER_FREDEMMOTT_HandTrackedCockpitClicking,
      },
      .mFacets = Literals {
        Facets::TransformsPoses,
      },
    },
    {
      .mID = XR_APILAYER_MBUCCHIA_vulkan_d3d12_interop,
      .mAbove = Literals {
        // Incompatible with Vulkan:
        XR_APILAYER_MBUCCHIA_toolkit,
        XR_APILAYER_NOVENDOR_OBSMirror,
      },
    },
    {
      .mID = XR_APILAYER_NOVENDOR_OBSMirror,
      .mBelow = Literals {
        Facets::CompositionLayers,
        XR_VARJO_foveated_rendering,
      },
    },
    {
      .mID = XR_APILAYER_NOVENDOR_XRNeckSafer,
      .mAbove = Literals {
        // Unknown incompatibility issue:
        XR_APILAYER_FREDEMMOTT_HandTrackedCockpitClicking,
        // - https://gitlab.com/NobiWan/xrnecksafer/-/issues/15
        // - https://gitlab.com/NobiWan/xrnecksafer/-/issues/16
        // - Other developers have mentioned thread safety issues in XRNS that can cause crashes; I've not confirmed these
        Facets::CompositionLayers,
      },
    },
  };
}

}// namespace FredEmmott::OpenXRLayers