// Copyright 2023 Fred Emmott <fred@fredemmott.com>
// SPDX-License-Identifier: ISC
#pragma once

#include <cassert>
#include <deque>
#include <format>
#include <string>
#include <unordered_map>

#include "ConstexprString.hpp"
#include "StringTemplateParameter.hpp"

namespace FredEmmott::OpenXRLayers {

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
    : Facet(Kind::Explicit, id, description) {}

  constexpr Facet(
    const Kind kind,
    const std::string_view& id,
    const std::string_view& description)
    : mKind(kind),
      mID(id),
      mDescription(description) {}

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
    static auto operator()(const Facet& facet) noexcept {
      return std::hash<std::string_view> {}(facet.GetID());
    }
  };

 private:
  Kind mKind;
  ConstexprString mID;
  ConstexprString mDescription;
};

template <Facet::Kind TKind, auto TDescriptionFormat = "{}"_tp>
class BasicFacetID {
 public:
  BasicFacetID() = delete;
  explicit constexpr BasicFacetID(const std::string_view id) : mID(id) {}

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

struct FacetTraceEntry {
  Facet mWhat;
  Facet mWhy;
  constexpr bool operator==(const FacetTraceEntry&) const noexcept = default;
};

// Really want a stack, but std::stack isn't iterable
using FacetTrace = std::deque<FacetTraceEntry>;
using FacetMap = std::unordered_map<Facet, FacetTrace, Facet::Hash>;

struct LayerRules {
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

std::vector<LayerRules> GetLayerRules();

}// namespace FredEmmott::OpenXRLayers