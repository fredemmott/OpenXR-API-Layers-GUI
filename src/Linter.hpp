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

using PathSet = std::set<std::filesystem::path>;

class APILayerStore;

class LintError {
 public:
  LintError() = delete;
  LintError(const std::string& description, const PathSet& affectedLayers);
  virtual ~LintError() = default;

  [[nodiscard]] std::string GetDescription() const;
  [[nodiscard]] PathSet GetAffectedLayers() const;

 private:
  std::string mDescription;
  PathSet mAffectedLayers;
};

// A lint error that can be automatically fixed
class FixableLintError : public LintError {
 public:
  using LintError::LintError;
  ~FixableLintError() override = default;

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
    const std::filesystem::path& layerToMove,
    Position position,
    const std::filesystem::path& relativeTo,
    const PathSet& allAffectedLayers = {});
  virtual ~OrderingLintError() = default;

  virtual std::vector<APILayer> Fix(const std::vector<APILayer>&) override;

 private:
  std::filesystem::path mLayerToMove;
  Position mPosition;
  std::filesystem::path mRelativeTo;
};

/// A lint error that is fixed by disabling the layer
class KnownBadLayerLintError final : public FixableLintError {
  public:
    KnownBadLayerLintError(
      const std::string& description,
      const std::filesystem::path& layer);

  ~KnownBadLayerLintError() override = default;
  std::vector<APILayer> Fix(const std::vector<APILayer>&) override;
};

/// A lint error that is fixed by removing the layer
class InvalidLayerLintError final : public FixableLintError {
 public:
  InvalidLayerLintError(
    const std::string& description,
    const std::filesystem::path& layer);
  virtual ~InvalidLayerLintError() = default;

  virtual std::vector<APILayer> Fix(const std::vector<APILayer>&) override;
};

// A lint error that is fixed by disabling the layer
class InvalidLayerStateLintError final : public FixableLintError {
 public:
  InvalidLayerStateLintError(
    const std::string& description,
    const std::filesystem::path& layer);
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
    const std::vector<std::tuple<APILayer, APILayerDetails>>&)
    = 0;
};

std::vector<std::shared_ptr<LintError>> RunAllLinters(
  const APILayerStore*,
  const std::vector<APILayer>&);

}// namespace FredEmmott::OpenXRLayers