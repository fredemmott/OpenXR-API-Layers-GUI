// Copyright 2023 Fred Emmott <fred@fredemmott.com>
// SPDX-License-Identifier: ISC

#pragma once
#include <vector>

#include "APILayer.hpp"

namespace FredEmmott::OpenXRLayers {

std::vector<APILayer> GetAPILayers();
bool SetAPILayers(const std::vector<APILayer>&);

}// namespace FredEmmott::OpenXRLayers
