// Copyright 2023 Fred Emmott <fred@fredemmott.com>
// SPDX-License-Identifier: ISC

#pragma once

#include <string>

namespace FredEmmott::OpenXRLayers::Config {

constexpr std::string_view BUILD_TARGET_ID {"@BUILD_TARGET_ID@"};

// clang-format off
constexpr bool USE_EMOJI { @USE_EMOJI_BOOL@ };
// clang-format on

constexpr auto GLYPH_STATE_OK = USE_EMOJI ? "👍" : "OK";
constexpr auto GLYPH_STATE_INVALID_LAYER = USE_EMOJI ? "🛑" : "L";
constexpr auto GLYPH_STATE_CONFLICT = USE_EMOJI ? "⚠️" : "C";

}// namespace FredEmmott::OpenXRLayers::Config