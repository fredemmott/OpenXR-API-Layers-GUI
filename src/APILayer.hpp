// Copyright 2023 Fred Emmott <fred@fredemmott.com>
// SPDX-License-Identifier: ISC

#pragma once

#include <filesystem>

namespace FredEmmott::OpenXRLayers {

/** The basic information about an API layer.
 *
 * This contains the information that is available in the list of
 * API layers, e.g. the Windows registry, not data from the manifest.
 *
 * Manifest data is available via `APILayerDetails`.
 */
struct APILayer {
  enum class Value {
    Enabled,
    Disabled,
    Win32_NotDWORD,
  };

  std::filesystem::path mJSONPath;
  Value mValue;

  constexpr bool IsEnabled() const noexcept {
    return mValue == Value::Enabled;
  };

  bool operator==(const APILayer&) const noexcept = default;
};

struct Extension {
  std::string mName;
  std::string mVersion;

  bool operator==(const Extension&) const noexcept = default;
};

/// Information from the API layer manifest
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
  std::string mAPIVersion;
  std::string mImplementationVersion;
  std::vector<Extension> mExtensions;

  bool operator==(const APILayerDetails&) const noexcept = default;

  std::string StateAsString() const noexcept;
};

}// namespace FredEmmott::OpenXRLayers
