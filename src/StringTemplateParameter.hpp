// Copyright 2023 Fred Emmott <fred@fredemmott.com>
// SPDX-License-Identifier: ISC
#pragma once

#include <ranges>

namespace FredEmmott::OpenXRLayers {

/** Class containing a compile-time string literal, which is usable as a
 * template parameter.
 *
 * C++ doesn't allow string literals or `std::string_view`'s as template
 * parameters, so we need a helper.
 *
 * Usage:
 *
 *     "foo"_tp
 */
template <size_t N>
struct StringTemplateParameter {
  StringTemplateParameter() = delete;
  // ReSharper disable once CppNonExplicitConvertingConstructor
  consteval StringTemplateParameter(char const (&init)[N]) {
    std::ranges::copy(init, value);
  }

  char value[N] {};
};

template <>
struct StringTemplateParameter<0> {};

/// Make a string literal usable as a template parameter
template <StringTemplateParameter T>
consteval auto operator""_tp() {
  return T;
}

}// namespace FredEmmott::OpenXRLayers