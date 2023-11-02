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

class LintError {
 public:
  LintError() = delete;
  LintError(const std::string& description, const PathSet& affectedLayers);
  virtual ~LintError() = default;

  std::string GetDescription() const;
  PathSet GetAffectedLayers() const;

 private:
  std::string mDescription;
  PathSet mAffectedLayers;
};

class FixableLintError : public LintError {
 public:
  using LintError::LintError;
  virtual std::vector<APILayer> Fix(const std::vector<APILayer>&) = 0;
};

// A lint error that is fixed by removing the error
class InvalidLayerLintError final : public FixableLintError {
 public:
  InvalidLayerLintError(
    const std::string& description,
    const std::filesystem::path& layer);

  virtual std::vector<APILayer> Fix(const std::vector<APILayer>&) override;
};

class Linter {
 protected:
  Linter();

 public:
  virtual ~Linter();

  virtual std::vector<std::shared_ptr<LintError>> Lint(
    const std::vector<std::tuple<APILayer, APILayerDetails>>&)
    = 0;
};

std::vector<std::shared_ptr<LintError>> RunAllLinters(
  const std::vector<APILayer>&);

}// namespace FredEmmott::OpenXRLayers