// Copyright 2023 Fred Emmott <fred@fredemmott.com>
// SPDX-License-Identifier: ISC

#include <fmt/core.h>
#include <nlohmann/json.hpp>

#include <fstream>

#include "APILayer.hpp"

namespace FredEmmott::OpenXRLayers {

APILayerDetails::APILayerDetails(const std::filesystem::path& jsonPath) {
  if (!std::filesystem::exists(jsonPath)) {
    mState = State::NoJsonFile;
    return;
  }

  std::ifstream f(jsonPath);
  if (!f) {
    mState = State::UnreadableJsonFile;
    return;
  }

  nlohmann::json json;
  try {
    json = nlohmann::json::parse(f);
  } catch (const std::runtime_error&) {
    mState = State::InvalidJson;
    return;
  }

  mFileFormatVersion = json.value("file_format_version", std::string {});
  if (!json.contains("api_layer")) {
    mState = State::MissingData;
    return;
  }
  auto layer = json.at("api_layer");

  mName = layer.value("name", std::string {});

  auto libraryPath
    = std::filesystem::path(layer.value("library_path", std::string {}));
  if (!libraryPath.empty()) {
    if (libraryPath.is_absolute()) {
      mLibraryPath = libraryPath;
    } else {
      libraryPath = jsonPath.parent_path() / libraryPath;
      if (std::filesystem::exists(libraryPath)) {
        mLibraryPath = std::filesystem::canonical(libraryPath);
      } else {
        mLibraryPath = std::filesystem::weakly_canonical(libraryPath);
      }
    }
  }

  mDescription = layer.value("description", std::string {});
  if (layer.contains("instance_extensions")) {
    auto extensions = layer.at("instance_extensions");
    if (extensions.is_array()) {
      for (const auto& extension: extensions) {
        std::string version;
        if (extension.contains("extension_version")) {
          auto it = extension.at("extension_version");
          if (it.is_string()) {
            version = it;
          } else if (it.is_number_integer()) {
            version = fmt::format("{}", it.get<uint32_t>());
          }
        }
        mExtensions.push_back(Extension {
          .mName = extension.value("name", std::string {}),
          .mVersion = version,
        });
      }
    }
  }

  mState = State::Loaded;
}

std::string APILayerDetails::StateAsString() const noexcept {
  switch (mState) {
    case State::Loaded:
      return "Loaded";
    case State::Uninitialized:
      return "Internal error";
    case State::NoJsonFile:
      return "The file does not exist";
    case State::UnreadableJsonFile:
      return "The JSON file is unreadable";
    case State::InvalidJson:
      return "The file does not contain valid JSON";
    case State::MissingData:
      return "The file does not contain data required by OpenXR";
    default:
      return fmt::format(
        "Internal error ({})",
        static_cast<std::underlying_type_t<APILayerDetails::State>>(mState));
  }
}

}// namespace FredEmmott::OpenXRLayers