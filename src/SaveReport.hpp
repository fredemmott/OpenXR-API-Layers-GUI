// Copyright 2023 Fred Emmott <fred@fredemmott.com>
// SPDX-License-Identifier: ISC

#pragma once

#include <filesystem>

namespace FredEmmott::OpenXRLayers {

std::string GenerateReport();
void SaveReport(const std::filesystem::path&);

}// namespace FredEmmott::OpenXRLayers