// Copyright 2023 Fred Emmott <fred@fredemmott.com>
// SPDX-License-Identifier: ISC

#pragma once

#include <Windows.h>

namespace FredEmmott::OpenXRLayers::Config {

// Can't use constexpr as HKEY_* are defined as pointer values,
// even though they're literal values.
// clang-format off
const auto APILAYER_HKEY_ROOT { @APILAYER_HKEY_ROOT@ };
// clang-format on

}