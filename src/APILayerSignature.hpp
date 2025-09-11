// Copyright 2025 Fred Emmott <fred@fredemmott.com>
// SPDX-License-Identifier: MIT
#pragma once

#include <chrono>
#include <string>

namespace FredEmmott::OpenXRLayers {

struct APILayerSignature {
  enum class Error {
    NotSupported,
    FilesystemError,
    Unsigned,
    UntrustedSignature,
    Expired,
  };

  std::string mSignedBy;
  std::chrono::system_clock::time_point mSignedAt;
};
}// namespace FredEmmott::OpenXRLayers