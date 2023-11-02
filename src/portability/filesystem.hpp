// Copyright 2023 Fred Emmott <fred@fredemmott.com>
// SPDX-License-Identifier: ISC

#pragma once

#include <filesystem>
#include <utility>

namespace FredEmmott::PortabilityHacks {
template <class T>
concept Hashable = requires(T a) {
  { std::hash<T> {}(a) } -> std::convertible_to<size_t>;
};

template <class T>
concept NotHashable = !Hashable<T>;
}// namespace FredEmmott::PortabilityHacks

// std::hash<std::filesystem::path> is available on MSVC, but not MacOS
template <class T>
  requires requires(T x) {
    { x } -> std::convertible_to<std::filesystem::path>;
    FredEmmott::PortabilityHacks::NotHashable<T>;
  }
struct std::hash<T> {
  std::size_t operator()(const T& p) const {
    return hash_value(p);
  }
};