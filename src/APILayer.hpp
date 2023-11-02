// Copyright 2023 Fred Emmott <fred@fredemmott.com>
// SPDX-License-Identifier: ISC

#pragma once

#include <filesystem>

namespace FredEmmott::OpenXRLayers {

struct APILayer {
  std::filesystem::path mJSONPath;
  bool mIsEnabled {};

  auto operator<=>(const APILayer&) const noexcept = default;
};

struct Extension {
  std::string mName;
  std::string mVersion;

  auto operator<=>(const Extension&) const noexcept = default;
};

struct APILayerDetails {
  APILayerDetails(const std::filesystem::path& jsonPath);
  enum class State {
    Uninitialized,
    NoJsonFile,
    UnreadableJsonFile,
    InvalidJson,
    MissingData,
    Loaded,
  };
  State mState {State::Uninitialized};

  std::string mFileFormatVersion;
  std::string mName;
  std::filesystem::path mLibraryPath;
  std::string mDescription;
  std::vector<Extension> mExtensions;

  auto operator<=>(const APILayerDetails&) const noexcept = default;
};

}// namespace FredEmmott::OpenXRLayers
