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
  std::vector<APILayer> layers {};
  APILayer* selectedLayer {nullptr};
  bool reloadLayers {true};

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
  while (window.isOpen()) {
    if (reloadLayers) {
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
      reloadLayers = false;
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

        auto label = layer.mJSONPath.string();

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
              reloadLayers = true;
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
      reloadLayers = true;
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
        selectedLayer->mJSONPath.string().c_str());
      ImGui::Separator();
      ImGui::SetCursorPosX(256 + 128);
      if (ImGui::Button("Remove", {64, 0}) && selectedLayer) {
        auto nextLayers = layers;
        auto it = std::ranges::find(nextLayers, *selectedLayer);
        if (it != nextLayers.end()) {
          nextLayers.erase(it);
          SetAPILayers(nextLayers);
          reloadLayers = true;
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
        reloadLayers = true;
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
        reloadLayers = true;
      }
    }
    ImGui::EndDisabled();
    ImGui::EndGroup();

    ImGui::SetNextItemWidth(1024);
    if (ImGui::BeginTabBar("##Info", ImGuiTabBarFlags_None)) {
      ImGui::BeginTabItem("Details");
      if (selectedLayer) {
        ImGui::BeginTable(
          "##DetailsTable",
          2,
          ImGuiTableFlags_Borders | ImGuiTableFlags_SizingFixedFit);

        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::Text("JSON File");
        ImGui::TableNextColumn();
        if (ImGui::Button("Copy##CopyJSONFile")) {
          ImGui::SetClipboardText(selectedLayer->mJSONPath.string().c_str());
        }
        ImGui::SameLine();
        ImGui::Text("%s", selectedLayer->mJSONPath.string().c_str());

        const APILayerDetails details {selectedLayer->mJSONPath};
        if (details.mState != APILayerDetails::State::Loaded) {
          std::string error;
          using State = APILayerDetails::State;
          switch (details.mState) {
            case State::Uninitialized:
              error = "Internal error";
              break;
            case State::NoJsonFile:
              error = "The file does not exist";
              break;
            case State::UnreadableJsonFile:
              error = "The JSON file is unreadable";
              break;
            case State::InvalidJson:
              error = "The file does not contain valid JSON";
              break;
            case State::MissingData:
              error = "The file does not contain data required by OpenXR";
              break;
            default:
              error = std::format(
                "Internal error ({})",
                static_cast<std::underlying_type_t<APILayerDetails::State>>(
                  details.mState));
              break;
          }

          ImGui::TableNextRow();
          ImGui::TableNextColumn();
          ImGui::Text("%s", Config::GLYPH_ERROR);
          ImGui::TableNextColumn();
          ImGui::Text("%s", error.c_str());
        } else /* have details */ {
          {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::Text("Library Path");
            ImGui::TableNextColumn();
            if (details.mLibraryPath.empty()) {
              ImGui::Text(
                "%s", std::format("{} [none]", Config::GLYPH_ERROR).c_str());
            } else {
              auto text = details.mLibraryPath.string();
              if (!std::filesystem::exists(details.mLibraryPath)) {
                text = std::format("{} {}", Config::GLYPH_ERROR, text);
              }
              if (ImGui::Button("Copy##LibraryPath")) {
                ImGui::SetClipboardText(details.mLibraryPath.string().c_str());
              }
              ImGui::SameLine();
              ImGui::Text("%s", details.mLibraryPath.string().c_str());
            }
          }

          if (!details.mName.empty()) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::Text("Name");
            ImGui::TableNextColumn();
            if (ImGui::Button("Copy##Name")) {
              ImGui::SetClipboardText(details.mName.c_str());
            }
            ImGui::SameLine();
            ImGui::Text("%s", details.mName.c_str());
          }

          if (!details.mDescription.empty()) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::Text("Description");
            ImGui::TableNextColumn();
            ImGui::Text("%s", details.mDescription.c_str());
          }

          if (!details.mFileFormatVersion.empty()) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::Text("File Format Version");
            ImGui::TableNextColumn();
            ImGui::Text("%s", details.mFileFormatVersion.c_str());
          }

          if (!details.mExtensions.empty()) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::Text("Extensions");
            ImGui::TableNextColumn();
            ImGui::BeginTable(
              "##ExtensionsTable",
              2,
              ImGuiTableFlags_Borders | ImGuiTableFlags_SizingFixedFit);
            ImGui::TableSetupColumn("Name");
            ImGui::TableSetupColumn("Version");
            ImGui::TableHeadersRow();
            for (const auto& ext: details.mExtensions) {
              ImGui::TableNextRow();
              ImGui::TableNextColumn();
              ImGui::Text("%s", ext.mName.c_str());
              ImGui::TableNextColumn();
              ImGui::Text("%s", ext.mVersion.c_str());
            }
            ImGui::EndTable();
            ImGui::TableNextColumn();
          }
        }

        ImGui::EndTable();
      } else {
        ImGui::BeginDisabled();
        ImGui::Text("Select a layer above for details.");
        ImGui::EndDisabled();
      }
      ImGui::EndTabItem();
      ImGui::EndTabBar();
    }

    ImGui::End();

    ImGui::SFML::Render(window);
    window.display();
  }
  ImGui::SFML::Shutdown();
}

}// namespace FredEmmott::OpenXRLayers::GUI
