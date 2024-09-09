// Copyright 2023 Fred Emmott <fred@fredemmott.com>
// SPDX-License-Identifier: ISC

#pragma once

#include <string>

namespace FredEmmott::OpenXRLayers::Config {

constexpr auto BUILD_VERSION {"@BUILD_VERSION_STRING@"};
constexpr auto BUILD_VERSION_W {L"@BUILD_VERSION_STRING@"};

// clang-format off
constexpr bool USE_EMOJI { @USE_EMOJI_BOOL@ };
// clang-format on

constexpr auto GLYPH_ENABLED {USE_EMOJI ? "\u2705" : "Y"};
constexpr auto GLYPH_DISABLED {USE_EMOJI ? "\u274c" : "N"};
constexpr auto GLYPH_ERROR {USE_EMOJI ? "\u26a0" : "!"};

constexpr auto LICENSE_TEXT {R"---LICENSE---(@LICENSE_TEXT@)---LICENSE---"};

}// namespace FredEmmott::OpenXRLayers::Config