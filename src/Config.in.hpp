// Copyright 2023 Fred Emmott <fred@fredemmott.com>
// SPDX-License-Identifier: ISC

#pragma once

#include <string>

namespace FredEmmott::OpenXRLayers::Config {

constexpr std::string BUILD_TARGET_ID {"@BUILD_TARGET_ID"};
// clang-format off
constexpr bool USE_EMOJI { @USE_EMOJI@ };
// clang-format on

constexpr auto GLYPH_ENABLED = USE_EMOJI ? "✅" : "Y";
constexpr auto GLYPH_DISABLED = USE_EMOJI ? "❌" : "N";
constexpr auto GLYPH_CRITICAL_ERROR = "C";
constexpr auto GLYPH_WARNING = "W";

}// namespace FredEmmott::OpenXRLayers::Config