// Copyright 2023 Fred Emmott <fred@fredemmott.com>
// SPDX-License-Identifier: ISC

#pragma once

#include <filesystem>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "APILayer.hpp"

namespace FredEmmott::OpenXRLayers {

using LayerKeySet = std::set<APILayer::Key>;

class APILayerStore;

class LintError {
 public:
  LintError() = delete;
  LintError(const std::string& description, const LayerKeySet& affectedLayers);
  virtual ~LintError() = default;

  [[nodiscard]] std::string GetDescription() const;
  [[nodiscard]] LayerKeySet GetAffectedLayers() const;

 private:
  std::string mDescription;
  LayerKeySet mAffectedLayers;
};

// A lint error that can be automatically fixed
class FixableLintError : public LintError {
 public:
  using LintError::LintError;
  ~FixableLintError() override = default;

  [[nodiscard]]
  virtual bool IsFixable() const noexcept {
    return true;
  }
  [[nodiscard]]
  virtual std::vector<APILayer> Fix(const std::vector<APILayer>&) = 0;
};

// A lint error that can be fixed by reordering layers
class OrderingLintError final : public FixableLintError {
 public:
  enum class Position {
    Above,
    Below,
  };

  OrderingLintError(
    const std::string& description,
    const APILayer& layerToMove,
    Position position,
    const APILayer& relativeTo,
    const LayerKeySet& allAffectedLayers = {});
  virtual ~OrderingLintError() = default;

  virtual std::vector<APILayer> Fix(const std::vector<APILayer>&) override;

 private:
  APILayer::Key mLayerToMove;
  Position mPosition;
  APILayer::Key mRelativeTo;
};

/// A lint error that is fixed by disabling the layer
class KnownBadLayerLintError final : public FixableLintError {
 public:
  KnownBadLayerLintError(const std::string& description, const APILayer& layer);

  ~KnownBadLayerLintError() override = default;
  std::vector<APILayer> Fix(const std::vector<APILayer>&) override;
};

/// A lint error that is fixed by removing the layer
class InvalidLayerLintError final : public FixableLintError {
 public:
  InvalidLayerLintError(const std::string& description, const APILayer& layer);
  ~InvalidLayerLintError() override = default;

  bool IsFixable() const noexcept override;
  std::vector<APILayer> Fix(const std::vector<APILayer>&) override;

 private:
  bool mIsFixable;
};

// A lint error that is fixed by disabling the layer
class InvalidLayerStateLintError final : public FixableLintError {
 public:
  InvalidLayerStateLintError(
    const std::string& description,
    const APILayer& layer);
  virtual ~InvalidLayerStateLintError() = default;

  virtual std::vector<APILayer> Fix(const std::vector<APILayer>&) override;
};

class Linter {
 protected:
  Linter();

 public:
  virtual ~Linter();

  virtual std::vector<std::shared_ptr<LintError>> Lint(
    const APILayerStore*,
    const std::vector<std::tuple<APILayer, APILayerDetails>>&) = 0;
};

std::vector<std::shared_ptr<LintError>> RunAllLinters(
  const APILayerStore*,
  const std::vector<APILayer>&);

}// namespace FredEmmott::OpenXRLayers