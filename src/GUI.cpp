// Copyright 2023 Fred Emmott <fred@fredemmott.com>
// SPDX-License-Identifier: ISC

#include "GUI.hpp"

#include "APILayer.hpp"
#include "APILayers.hpp"
#include "imgui-SFML.h"
#include "imgui.h"

#include <SFML/Graphics/RenderWindow.hpp>
#include <SFML/System/Clock.hpp>
#include <SFML/Window/Event.hpp>

#include <iostream>

namespace FredEmmott::OpenXRLayers::GUI {

void Run() {
  auto layers = GetAPILayers();

  sf::RenderWindow window {sf::VideoMode(640, 480), "OpenXR Layers GUI"};
  window.setFramerateLimit(60);
  if (!ImGui::SFML::Init(window)) {
    return;
  }

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

    ImGui::BeginTable("Installed Layers", 3);
    for (const auto& layer: layers) {
      ImGui::TableNextRow();
      // Enabled?
      ImGui::TableNextColumn();
      ImGui::Text("Y");
      // Errors/Warning
      ImGui::TableNextColumn();
      ImGui::Text("?");
      ImGui::TableNextColumn();
      ImGui::Text("%s", layer.mPath.string().c_str());
    }
    ImGui::EndTable();
    ImGui::End();

    ImGui::SFML::Render(window);
    window.display();
  }
  ImGui::SFML::Shutdown();
}

}// namespace FredEmmott::OpenXRLayers::GUI
