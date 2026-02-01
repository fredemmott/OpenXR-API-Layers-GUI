// Copyright 2026 Fred Emmott <fred@fredemmott.com>
// SPDX-License-Identifier: MIT
#pragma once
#include <utility>

namespace FredEmmott::OpenXRLayers {

enum class Architecture : uint8_t {
  Invalid = 0,
  x86 = 1 << 0,
  x64 = 1 << 1,
};

class Architectures {
  Architecture mValue;

 public:
  constexpr Architectures() = default;
  constexpr Architectures(const Architecture value) : mValue(value) {}

  constexpr Architectures& operator|=(const Architecture other) {
    mValue = static_cast<Architecture>(
      std::to_underlying(mValue) | std::to_underlying(other));
    return *this;
  }

  constexpr Architectures& operator|=(const Architectures other) {
    return (*this |= other.mValue);
  }

  [[nodiscard]]
  constexpr bool contains(const Architecture value) const noexcept {
    const auto bits = std::to_underlying(value);
    return (std::to_underlying(mValue) & bits) == bits;
  }

  [[nodiscard]]
  constexpr bool contains(const Architectures other) const noexcept {
    return contains(other.mValue);
  }

  auto underlying() const noexcept {
    return std::to_underlying(mValue);
  }

  constexpr auto operator<=>(const Architectures&) const = default;
};

constexpr Architectures operator|(
  const Architecture lhs,
  const Architecture rhs) {
  return Architectures {lhs} |= rhs;
}

}// namespace FredEmmott::OpenXRLayers