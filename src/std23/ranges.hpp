// Copyright 2023 Fred Emmott <fred@fredemmott.com>
// SPDX-License-Identifier: ISC

#pragma once

#include <ranges>

/** Basic functionality from C++23
 *
 * This doesn't aim to be a complete or conformant polyfill, just enough for
 * basic usage
 */
namespace FredEmmott::OpenXRLayers::std23::ranges {
template <template <class...> class C, class... Args>
class to {
 public:
  template <std::ranges::range R>
  constexpr auto operator()(R&& r) const {
    return C<std::ranges::range_value_t<R>, Args...> {
      std::ranges::begin(r),
      std::ranges::end(r),
    };
  }
};
template <std::ranges::range R, template <class...> class C, class... Args>
constexpr auto operator|(R&& r, to<C, Args...> const& t) {
  return t(std::forward<R>(r));
}

template <std::ranges::range R, class Proj = std::identity>
constexpr bool contains(R&& r, const auto& value, Proj proj = Proj {}) {
  return std::ranges::find(r, value, proj) != std::ranges::end(r);
}
}// namespace FredEmmott::OpenXRLayers::std23::ranges
