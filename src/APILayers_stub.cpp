// Copyright 2023 Fred Emmott <fred@fredemmott.com>
// SPDX-License-Identifier: ISC

#include "APILayers.hpp"

namespace FredEmmott::OpenXRLayers {

std::vector<APILayer> GetAPILayers() {
  return {
    APILayer {
      .mPath = {"/var/not_a_real_layer.json"},
      .mIsEnabled = true,
    },
  };
}

void SetAPILayers(const std::vector<APILayer>&) {
}

}// namespace FredEmmott::OpenXRLayers