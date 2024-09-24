// Copyright 2023 Fred Emmott <fred@fredemmott.com>
// SPDX-License-Identifier: ISC

#pragma once

#include <cassert>
#include <format>
#include <string>
#include <unordered_map>
#include <variant>

namespace FredEmmott::OpenXRLayers {

class ConstexprString {
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

class Facet {
 public:
  enum class Kind {
    Explicit,
    Layer,
    Extension,
  };

  Facet() = delete;
  constexpr Facet(
    const std::string_view& id,
    const std::string_view& description)
    : Facet(Kind::Explicit, id, description) {
  }

  constexpr Facet(
    const Kind kind,
    const std::string_view& id,
    const std::string_view& description)
    : mKind(kind), mID(id), mDescription(description) {
  }

  [[nodiscard]] constexpr auto GetKind() const noexcept {
    return mKind;
  }

  [[nodiscard]] constexpr auto GetID() const noexcept {
    return mID.Get();
  }

  [[nodiscard]] constexpr std::string_view GetDescription() const noexcept {
    return mDescription.Get();
  }

  [[nodiscard]] constexpr bool operator==(const Facet&) const noexcept
    = default;

  struct Hash {
    auto operator()(const Facet& facet) const noexcept {
      return std::hash<std::string_view> {}(facet.GetID());
    }
  };

 private:
  Kind mKind;
  ConstexprString mID;
  ConstexprString mDescription;
};

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

// Compile-time literal string suffix
template <StringTemplateParameter T>
consteval auto operator""_tp() {
  return T;
}

template <Facet::Kind TKind, auto TDescriptionFormat = "{}"_tp>
class BasicFacetID {
 public:
  BasicFacetID() = delete;
  explicit constexpr BasicFacetID(const std::string_view id) : mID(id) {
  }

  explicit constexpr BasicFacetID(const Facet& facet) : mID(facet.GetID()) {
    assert(facet.GetKind() == TKind);
  }

  [[nodiscard]] constexpr auto GetID() const noexcept {
    return mID.Get();
  }

  // ReSharper disable once CppNonExplicitConversionOperator
  constexpr operator Facet() const noexcept {// NOLINT(*-explicit-constructor)
    return Facet {
      TKind, mID.Get(), std::format(TDescriptionFormat.value, mID.Get())};
  }

  [[nodiscard]] constexpr bool operator==(const BasicFacetID&) const noexcept
    = default;

  [[nodiscard]] constexpr bool operator==(const Facet& facet) const noexcept {
    return facet.GetKind() == TKind && facet.GetID() == mID.Get();
  }

 private:
  ConstexprString mID;
};

using LayerID = BasicFacetID<Facet::Kind::Layer>;
using ExtensionID = BasicFacetID<Facet::Kind::Extension, "provides {}"_tp>;

namespace Facets {
constexpr Facet CompositionLayers {
  "#compositionLayers",
  "provides an overlay",
};
}// namespace Facets

using FacetTrace = std::vector<Facet>;
using FacetMap = std::unordered_map<Facet, FacetTrace, Facet::Hash>;

struct KnownLayer {
  const Facet mID;
  /* Features that should be below this layer.
   * A 'feature' can include:
   * - an API layer name
   * - an extension name
   * - an explicit feature ID
   */
  FacetMap mAbove;

  /* Features that should be above this layer.
   *
   * @see mAbove
   */
  FacetMap mBelow;

  /* Features that this layer provides, in addition to its name and extensions.
   *
   * This should only include constants from the Features namespace; extensions
   * should be specified in the OpenXR JSON manifest file, not here.
   */
  FacetMap mFacets;

  /* Features (usually other layers) that this layer is completely
   * incompatible with. */
  FacetMap mConflicts;

  /* Features (usually other layers) that this layer is completely
   * incompatible with, but one or both support enabling/disabling per game. */
  FacetMap mConflictsPerApp;
};

std::unordered_map<std::string, KnownLayer> GetKnownLayers();

}// namespace FredEmmott::OpenXRLayers