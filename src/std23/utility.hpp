// Copyright 2025 Fred Emmott <fred@fredemmott.com>
// SPDX-License-Identifier: MIT
#pragma once

#include <type_traits>

namespace FredEmmott::OpenXRLayers::std23 {

template <class T>
  requires std::is_enum_v<T>
auto to_underlying(T v) {
  return static_cast<std::underlying_type_t<T>>(v);
}

}// namespace FredEmmott::OpenXRLayers::std23