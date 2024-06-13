// Copyright 2023 Fred Emmott <fred@fredemmott.com>
// SPDX-License-Identifier: ISC

#pragma once

#include <string>

namespace FredEmmott::OpenXRLayers::Config {

constexpr std::string_view BUILD_TARGET_ID {"@BUILD_TARGET_ID@"};
constexpr std::string_view BUILD_VERSION {"@BUILD_VERSION_STRING@"};

// clang-format off
constexpr bool USE_EMOJI { @USE_EMOJI_BOOL@ };
// clang-format on

constexpr auto GLYPH_ERROR {USE_EMOJI ? "\u26a0" : "!"};

constexpr auto LICENSE_TEXT {R"---LICENSE---(@LICENSE_TEXT@)---LICENSE---"};

}// namespace FredEmmott::OpenXRLayers::Config