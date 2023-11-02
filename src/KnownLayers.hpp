// Copyright 2023 Fred Emmott <fred@fredemmott.com>
// SPDX-License-Identifier: ISC

#pragma once

#include <set>
#include <string>
#include <unordered_map>

namespace FredEmmott::OpenXRLayers {

struct KnownLayer {
  const std::string mName;
  /* Features used by this layer.
   *
   * A 'feature' can include:
   * - an API layer name
   * - an extension name
   */
  const std::set<std::string> mConsumes;
};

std::unordered_map<std::string, KnownLayer> GetKnownLayers();

}