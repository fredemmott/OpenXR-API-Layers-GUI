// Copyright 2023 Fred Emmott <fred@fredemmott.com>
// SPDX-License-Identifier: ISC

#pragma once

#include <expected>
#include <filesystem>

#include "APILayerSignature.hpp"

namespace FredEmmott::OpenXRLayers {

class APILayerStore;

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
    NotInstalled,
    Win32_NotDWORD,
  };
  enum class Kind {
    Explicit,
    Implicit,
  };

  APILayer(
    const APILayerStore* source,
    const std::filesystem::path& manifestPath,
    const Value value)
    : mSource(source),
      mManifestPath(manifestPath),
      mValue(value) {}

  static APILayer MakeAbsentExplicit(const std::string_view name) {
    APILayer ret {nullptr, {}, Value::NotInstalled};
    ret.mReferencedName = name;
    return ret;
  }

  Kind GetKind() const noexcept;

  const APILayerStore* mSource {nullptr};
  std::filesystem::path mManifestPath;
  Value mValue;

  /// Only set for explicit API layers that are referenced but not
  /// present
  std::string mReferencedName;

  [[nodiscard]]
  constexpr bool IsEnabled() const noexcept {
    return mValue == Value::Enabled;
  };

  bool operator==(const APILayer&) const noexcept = default;

 private:
  APILayer() = default;
};

struct Extension {
  std::string mName;
  std::string mVersion;

  bool operator==(const Extension&) const noexcept = default;
};

/// Information from the API layer manifest
struct APILayerDetails {
  APILayerDetails() = delete;
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

  std::expected<APILayerSignature, APILayerSignature::Error> mSignature;

  std::string mFileFormatVersion;
  std::string mName;
  std::filesystem::path mLibraryPath;
  std::string mDescription;
  std::string mAPIVersion;
  std::string mImplementationVersion;
  std::string mDisableEnvironment;
  std::string mEnableEnvironment;
  std::vector<Extension> mExtensions;

  std::filesystem::file_time_type mManifestFilesystemChangeTime;
  std::filesystem::file_time_type mLibraryFilesystemChangeTime;

  bool operator==(const APILayerDetails&) const noexcept = default;

  [[nodiscard]]
  std::string StateAsString() const noexcept;
};

}// namespace FredEmmott::OpenXRLayers
