// Copyright 2023 Fred Emmott <fred@fredemmott.com>
// SPDX-License-Identifier: ISC

#pragma once

#include <string>

namespace FredEmmott::OpenXRLayers::Config {

constexpr std::string_view BUILD_TARGET_ID {"@BUILD_TARGET_ID@"};
constexpr std::string_view BUILD_VERSION {"@CMAKE_PROJECT_VERSION@"};

// clang-format off
constexpr bool USE_EMOJI { @USE_EMOJI_BOOL@ };
// clang-format on

constexpr auto GLYPH_ERROR {USE_EMOJI ? "\u26a0" : "!"};

}// namespace FredEmmott::OpenXRLayers::Config