// Copyright 2023 Fred Emmott <fred@fredemmott.com>
// SPDX-License-Identifier: ISC

#include <filesystem>

#include <ShlObj.h>
#include <imgui_freetype.h>

#include "GUI.hpp"
#include <imgui-SFML.h>

namespace FredEmmott::OpenXRLayers::GUI {

void SetupFonts(ImGuiIO* io) {
  wchar_t* fontsPathStr {nullptr};
  if (
    SHGetKnownFolderPath(FOLDERID_Fonts, KF_FLAG_DEFAULT, NULL, &fontsPathStr)
    != S_OK) {
    return;
  }
  if (!fontsPathStr) {
    return;
  }

  std::filesystem::path fontsPath {fontsPathStr};

  io->Fonts->Clear();
  io->Fonts->AddFontFromFileTTF(
    (fontsPath / "segoeui.ttf").string().c_str(), 16.0f);

  static ImWchar ranges[] = {0x1, 0x1ffff, 0};
  static ImFontConfig config {};
  config.OversampleH = config.OversampleV = 1;
  config.MergeMode = true;
  config.FontBuilderFlags |= ImGuiFreeTypeBuilderFlags_LoadColor;

  io->Fonts->AddFontFromFileTTF(
    (fontsPath / "seguiemj.ttf").string().c_str(), 13.0f, &config, ranges);

  ImGui::SFML::UpdateFontTexture();
}

}// namespace FredEmmott::OpenXRLayers::GUI