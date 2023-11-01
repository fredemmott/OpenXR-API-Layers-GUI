// Copyright 2023 Fred Emmott <fred@fredemmott.com>
// SPDX-License-Identifier: ISC

#include "APILayers.hpp"

namespace FredEmmott::OpenXRLayers {

std::vector<APILayer> GetAPILayers() {
  return {
    APILayer {
      .mJSONPath = {"/var/not_a_real_layer.json"},
      .mIsEnabled = true,
    },
  };
}

bool SetAPILayers(const std::vector<APILayer>&) {
  return false;
}

}// namespace FredEmmott::OpenXRLayers