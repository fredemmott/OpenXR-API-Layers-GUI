// Copyright 2023 Fred Emmott <fred@fredemmott.com>
// SPDX-License-Identifier: ISC

#pragma once

#include <set>
#include <string>
#include <unordered_map>

namespace FredEmmott::OpenXRLayers {

namespace Features {
extern const char* Overlay;
}

struct KnownLayer {
  const std::string mName;
  /* Features that should be below this layer.
   *
   * A 'feature' can include:
   * - an API layer name
   * - an extension name
   * - a constant from the Features namespace
   */
  const std::set<std::string> mAbove;

  /* Features that should be above this layer.
   *
   * @see mAbove
   */
  const std::set<std::string> mBelow;

  /* Features that this layer provides, in addition to its' name and extensions.
   *
   * This should only include constants from the Features namespace; extensions
   * should be specified in the OpenXR JSON manifest file, not here.
   */
  const std::set<std::string> mProvides;

  /* Features (usually other layers) that this layer is completely
   * incompatible with. */
  const std::set<std::string> mConflicts;

  /* Features (usually other layers) that this layer is completely
   * incompatible with, but one or both support enabling/disabling per game. */
  const std::set<std::string> mConflictsPerApp;
};

std::unordered_map<std::string, KnownLayer> GetKnownLayers();

}// namespace FredEmmott::OpenXRLayers