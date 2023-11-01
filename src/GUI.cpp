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
  APILayer* selectedLayer {nullptr};

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
  bool layersChanged {false};
  while (window.isOpen()) {
    if (layersChanged) {
      layers = GetAPILayers();
      layersChanged = false;
      selectedLayer = nullptr;
    }
    sf::Event event {};
    while (window.pollEvent(event)) {
      ImGui::SFML::ProcessEvent(window, event);
      if (event.type == sf::Event::Closed) {
        window.close();
      }
    }

    ImGui::SFML::Update(window, deltaClock.restart());

    auto viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);
    ImGui::Begin(
      "MainWindow",
      0,
      ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove
        | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings
        | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar
        | ImGuiWindowFlags_NoScrollWithMouse);

    ImGui::BeginListBox("##Layers", {-FLT_MIN, 0});
    ImGuiListClipper clipper {};
    clipper.Begin(static_cast<int>(layers.size()));

    while (clipper.Step()) {
      for (auto i = clipper.DisplayStart; i < clipper.DisplayEnd; i++) {
        auto& layer = layers.at(i);
        ImGui::PushID(i);

        if (ImGui::Checkbox("##Enabled", &layer.mIsEnabled)) {
          SetAPILayers(layers);
        }

        auto label = layer.mPath.string();

        /* if (error) */ {
          label = std::format("{} {}", Config::GLYPH_ERROR, label);
        }

        ImGui::SameLine();

        if (ImGui::Selectable(label.c_str(), selectedLayer == &layer)) {
          selectedLayer = &layer;
        }

        if (ImGui::BeginDragDropSource()) {
          ImGui::SetDragDropPayload("APILayer*", &layer, sizeof(layer));
          ImGui::EndDragDropSource();
        }

        if (ImGui::BeginDragDropTarget()) {
          if (const auto payload = ImGui::AcceptDragDropPayload("APILayer*")) {
            const auto& dropped = *reinterpret_cast<APILayer*>(payload->Data);
            std::vector<APILayer> newLayers;
            for (const auto& it: layers) {
              if (it == dropped) {
                continue;
              }
              if (it == layer) {
                newLayers.push_back(dropped);
              }

              newLayers.push_back(it);
            }
            assert(layers.size() == newLayers.size());
            if (SetAPILayers(newLayers)) {
              layersChanged = true;
            }
          }
          ImGui::EndDragDropTarget();
        }

        ImGui::PopID();
      }
    }
    ImGui::EndListBox();

    ImGui::End();

    ImGui::SFML::Render(window);
    window.display();
  }
  ImGui::SFML::Shutdown();
}

}// namespace FredEmmott::OpenXRLayers::GUI
