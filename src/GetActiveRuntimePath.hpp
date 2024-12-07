// Copyright 2023 Fred Emmott <fred@fredemmott.com>
// SPDX-License-Identifier: ISC

#pragma once

#include <filesystem>

namespace FredEmmott::OpenXRLayers {

/** Return the path of the active runtime.
 *
 * Will return an empty path if the active runtime can not be determined.
 */
std::filesystem::path GetActiveRuntimePath();

}// namespace FredEmmott::OpenXRLayers