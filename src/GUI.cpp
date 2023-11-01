// Copyright 2023 Fred Emmott <fred@fredemmott.com>
// SPDX-License-Identifier: ISC

#include "GUI.hpp"

#include <SFML/Graphics/RenderWindow.hpp>
#include <SFML/System/Clock.hpp>
#include <SFML/Window/Event.hpp>

#include <format>
#include <iostream>

#include <imgui.h>

#include "APILayer.hpp"
#include "APILayers.hpp"
#include "Config.hpp"
#include <imgui-SFML.h>

namespace FredEmmott::OpenXRLayers::GUI {

void Run() {
  auto layers = GetAPILayers();
  std::filesystem::path selectedLayer;

  sf::RenderWindow window {
    sf::VideoMode(1024, 768),
    std::format("OpenXR Layers [{}]", Config::BUILD_TARGET_ID)};
  window.setFramerateLimit(60);
  if (!ImGui::SFML::Init(window)) {
    return;
  }

  auto& io = ImGui::GetIO();
  SetupFonts(&io);

  sf::Clock deltaClock {};
  while (window.isOpen()) {
    sf::Event event {};
    while (window.pollEvent(event)) {
      ImGui::SFML::ProcessEvent(window, event);
      if (event.type == sf::Event::Closed) {
        window.close();
      }
    }

    ImGui::SFML::Update(window, deltaClock.restart());

    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::Begin(
      "MainWindow",
      0,
      ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove
        | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings
        | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar
        | ImGuiWindowFlags_NoScrollWithMouse);

    ImGui::BeginTable(
      "Installed Layers", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg);

    ImGui::TableSetupColumn("Enabled", ImGuiTableColumnFlags_WidthFixed);
    ImGui::TableSetupColumn("Status", ImGuiTableColumnFlags_WidthFixed);
    ImGui::TableSetupColumn("Path");

    ImGui::TableHeadersRow();

    for (auto& layer: layers) {
      ImGui::TableNextRow();
      // Enabled?
      ImGui::TableNextColumn();
      if (ImGui::Checkbox(
            std::format("##Enabled/{}", layer.mPath.string()).c_str(),
            &layer.mIsEnabled)) {
        SetAPILayers(layers);
      }
      // Errors/Warning
      ImGui::TableNextColumn();
      ImGui::Text(Config::GLYPH_STATE_OK);
      // Path
      ImGui::TableNextColumn();
      if (ImGui::Selectable(
            layer.mPath.string().c_str(),
            layer.mPath == selectedLayer,
            ImGuiSelectableFlags_SpanAllColumns)) {
        selectedLayer = layer.mPath;
      }
    }
    ImGui::EndTable();
    ImGui::End();

    ImGui::SFML::Render(window);
    window.display();
  }
  ImGui::SFML::Shutdown();
}

}// namespace FredEmmott::OpenXRLayers::GUI
