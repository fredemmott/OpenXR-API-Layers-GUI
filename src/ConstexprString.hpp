// Copyright 2023 Fred Emmott <fred@fredemmott.com>
// SPDX-License-Identifier: ISC
#pragma once

#include <variant>

namespace FredEmmott::OpenXRLayers {

/** A compile-time or runtime string.
 *
 * If it is initialized at compile-time, it contains an `std::string_view`
 * reference to the compile-time constant.
 *
 * If it is initialized at runtime with a dynamic string, it stores a copy.
 *
 * This allows classes that contain strings to have both `constexpr` and safe
 * runtime instantiations.
 */
class ConstexprString final {
 public:
  ConstexprString() = delete;
  constexpr ConstexprString(std::string_view init) {
    if (std::is_constant_evaluated()) {
      mStorage = init;
    } else {
      mStorage = std::string {init};
    }
  }

  [[nodiscard]] constexpr std::string_view Get() const noexcept {
    if (std::holds_alternative<std::string_view>(mStorage)) {
      return std::get<std::string_view>(mStorage);
    }
    return std::get<std::string>(mStorage);
  }

  constexpr bool operator==(const ConstexprString& other) const noexcept
    = default;

 private:
  std::variant<std::string, std::string_view> mStorage;
};

}// namespace FredEmmott::OpenXRLayers