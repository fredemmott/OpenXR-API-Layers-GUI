// Copyright 2023 Fred Emmott <fred@fredemmott.com>
// SPDX-License-Identifier: ISC

#include "GUI.hpp"

#include <SFML/Graphics/RenderWindow.hpp>
#include <SFML/System/Clock.hpp>
#include <SFML/Window/Event.hpp>

#include <algorithm>
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
    std::format("OpenXR API Layers [{}]", Config::BUILD_TARGET_ID)};
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
      auto newLayers = GetAPILayers();
      if (selectedLayer) {
        auto it = std::ranges::find(newLayers, *selectedLayer);
        if (it != newLayers.end()) {
          selectedLayer = &*it;
        } else {
          selectedLayer = nullptr;
        }
      }
      layers = std::move(newLayers);
      layersChanged = false;
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

    ImGui::BeginListBox("##Layers", {768, 384});
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

    ImGui::SameLine();
    ImGui::BeginGroup();
    if (ImGui::Button("Reload List", {-FLT_MIN, 0})) {
      layersChanged = true;
    }
    ImGui::Button("Add Layer...", {-FLT_MIN, 0});
    ImGui::BeginDisabled(selectedLayer == nullptr);

    if (ImGui::Button("Remove Layer...", {-FLT_MIN, 0})) {
      ImGui::OpenPopup("Remove Layer");
    }

    ImGui::EndDisabled();

    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize({512, 0}, ImGuiCond_Appearing);
    if (ImGui::BeginPopupModal(
          "Remove Layer", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
      ImGui::TextWrapped(
        "Are you sure you want to completely remove '%s'?\n\nThis can not be "
        "undone.",
        selectedLayer->mPath.string().c_str());
      ImGui::Separator();
      ImGui::SetCursorPosX(256 + 128);
      if (ImGui::Button("Remove", {64, 0}) && selectedLayer) {
        auto nextLayers = layers;
        auto it = std::ranges::find(nextLayers, *selectedLayer);
        if (it != nextLayers.end()) {
          nextLayers.erase(it);
          SetAPILayers(nextLayers);
          layersChanged = true;
        }
        ImGui::CloseCurrentPopup();
      }
      ImGui::SameLine();
      if (ImGui::Button("Cancel", {64, 0})) {
        ImGui::CloseCurrentPopup();
      }
      ImGui::SetItemDefaultFocus();

      ImGui::EndPopup();
    }

    ImGui::Separator();

    ImGui::BeginDisabled(!(selectedLayer && !selectedLayer->mIsEnabled));
    if (ImGui::Button("Enable Layer", {-FLT_MIN, 0})) {
      selectedLayer->mIsEnabled = true;
      SetAPILayers(layers);
    }
    ImGui::EndDisabled();
    ImGui::BeginDisabled(!(selectedLayer && selectedLayer->mIsEnabled));
    if (ImGui::Button("Disable Layer", {-FLT_MIN, 0})) {
      selectedLayer->mIsEnabled = false;
      SetAPILayers(layers);
    }
    ImGui::EndDisabled();

    ImGui::Separator();

    ImGui::BeginDisabled(!(selectedLayer && *selectedLayer != layers.front()));
    if (ImGui::Button("Move Up", {-FLT_MIN, 0})) {
      auto newLayers = layers;
      auto it = std::ranges::find(newLayers, *selectedLayer);
      if (it != newLayers.begin() && it != newLayers.end()) {
        std::iter_swap((it - 1), it);
        SetAPILayers(newLayers);
        layersChanged = true;
      }
    }
    ImGui::EndDisabled();
    ImGui::BeginDisabled(!(selectedLayer && *selectedLayer != layers.back()));
    if (ImGui::Button("Move Down", {-FLT_MIN, 0})) {
      auto newLayers = layers;
      auto it = std::ranges::find(newLayers, *selectedLayer);
      if (it != newLayers.end() && (it + 1) != newLayers.end()) {
        std::iter_swap(it, it + 1);
        SetAPILayers(newLayers);
        layersChanged = true;
      }
    }
    ImGui::EndDisabled();
    ImGui::EndGroup();

    ImGui::End();

    ImGui::SFML::Render(window);
    window.display();
  }
  ImGui::SFML::Shutdown();
}

}// namespace FredEmmott::OpenXRLayers::GUI
