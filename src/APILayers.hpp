// Copyright 2023 Fred Emmott <fred@fredemmott.com>
// SPDX-License-Identifier: ISC

#pragma once
#include "APILayer.hpp"

#include <vector>

namespace FredEmmott::OpenXRLayers {

std::vector<APILayer> GetAPILayers();
void SetAPILayers(const std::vector<APILayer>&);

}// namespace FredEmmott::OpenXRLayers
