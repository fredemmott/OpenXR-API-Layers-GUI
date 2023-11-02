// Copyright 2023 Fred Emmott <fred@fredemmott.com>
// SPDX-License-Identifier: ISC

#include "GUI.hpp"

#include <SFML/Graphics/RenderWindow.hpp>
#include <SFML/System/Clock.hpp>
#include <SFML/Window/Event.hpp>

#include <algorithm>
#include <format>
#include <iostream>
#include <ranges>
#include <unordered_map>

#include <imgui.h>

#include "APILayer.hpp"
#include "APILayers.hpp"
#include "Config.hpp"
#include "Linter.hpp"
#include <imgui-SFML.h>

namespace FredEmmott::OpenXRLayers::GUI {

namespace {
constexpr ImVec2 MINIMUM_WINDOW_SIZE {1024, 768};

class MyWindow final : public sf::RenderWindow {
 public:
  using sf::RenderWindow::RenderWindow;
  virtual ~MyWindow() = default;

 protected:
  virtual void onResize() override {
    auto size = this->getSize();
    auto newSize = size;
    if (size.x < MINIMUM_WINDOW_SIZE.x) {
      newSize.x = MINIMUM_WINDOW_SIZE.x;
    }
    if (size.y < MINIMUM_WINDOW_SIZE.y) {
      newSize.y = MINIMUM_WINDOW_SIZE.y;
    }

    if (size == newSize) {
      return;
    }

    this->setSize(newSize);
  }
};
}// namespace

void Run() {
  using LintErrors = std::vector<std::shared_ptr<LintError>>;

  std::vector<APILayer> layers {};
  APILayer* selectedLayer {nullptr};
  LintErrors lintErrors;
  std::unordered_map<APILayer*, LintErrors> lintErrorsByLayer;
  bool reloadLayers {true};

  MyWindow window {
    sf::VideoMode(MINIMUM_WINDOW_SIZE.x, MINIMUM_WINDOW_SIZE.y),
    std::format(
      "OpenXR API Layers [{}] - v{}",
      Config::BUILD_TARGET_ID,
      Config::BUILD_VERSION)};
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

      std::unordered_map<std::filesystem::path, APILayer*> layersByPath;
      for (auto& layer: layers) {
        layersByPath.emplace(layer.mJSONPath, &layer);
      }
      lintErrors = RunAllLinters(layers);
      lintErrorsByLayer.clear();
      for (auto& error: lintErrors) {
        for (const auto& path: error->GetAffectedLayers()) {
          if (!layersByPath.contains(path)) {
            continue;
          }
          auto layer = layersByPath.at(path);
          if (lintErrorsByLayer.contains(layer)) {
            lintErrorsByLayer.at(layer).push_back(error);
          } else {
            lintErrorsByLayer[layer] = {error};
          }
        }
      }
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

    ImGui::BeginListBox(
      "##Layers", {viewport->WorkSize.x - 256, viewport->WorkSize.y / 2});
    ImGuiListClipper clipper {};
    clipper.Begin(static_cast<int>(layers.size()));

    while (clipper.Step()) {
      for (auto i = clipper.DisplayStart; i < clipper.DisplayEnd; i++) {
        auto& layer = layers.at(i);
        std::vector<std::shared_ptr<LintError>> layerErrors;
        if (lintErrorsByLayer.contains(&layer)) {
          layerErrors = lintErrorsByLayer.at(&layer);
        }

        ImGui::PushID(i);

        if (ImGui::Checkbox("##Enabled", &layer.mIsEnabled)) {
          SetAPILayers(layers);
        }

        auto label = layer.mJSONPath.string();

        if (!layerErrors.empty()) {
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

    ImGui::SetNextItemWidth(-FLT_MIN);
    if (ImGui::BeginTabBar("##Info", ImGuiTabBarFlags_None)) {
      if (ImGui::BeginTabItem("Warnings")) {
        ImGui::BeginChild("##ScrollArea", {-FLT_MIN, -FLT_MIN});
        if (selectedLayer) {
          ImGui::Text("For %s:", selectedLayer->mJSONPath.string().c_str());
        } else {
          ImGui::Text("All layers:");
        }

        LintErrors selectedErrors;
        if (selectedLayer) {
          if (lintErrorsByLayer.contains(selectedLayer)) {
            selectedErrors = lintErrorsByLayer.at(selectedLayer);
          }
        } else {
          selectedErrors = lintErrors;
        }

        ImGui::Indent();

        if (selectedErrors.empty()) {
          ImGui::Separator();
          ImGui::BeginDisabled();
          ImGui::Text("No warnings.");
          ImGui::EndDisabled();
        } else {
          std::vector<std::shared_ptr<FixableLintError>> fixableErrors;
          for (const auto& error: selectedErrors) {
            auto fixable = std::dynamic_pointer_cast<FixableLintError>(error);
            if (fixable) {
              fixableErrors.push_back(fixable);
            }
          }

          if (fixableErrors.size() > 1) {
            ImGui::AlignTextToFramePadding();
            if (fixableErrors.size() == selectedErrors.size()) {
              ImGui::Text(
                "All %llu warnings are automatically fixable:",
                fixableErrors.size());
            } else {
              ImGui::Text(
                "%llu out of %llu warnings are automatically "
                "fixable:",
                fixableErrors.size(),
                selectedErrors.size());
            }
            ImGui::SameLine();
            if (ImGui::Button("Fix Them!")) {
              auto nextLayers = layers;
              for (auto& fixable: fixableErrors) {
                nextLayers = fixable->Fix(nextLayers);
              }
              SetAPILayers(nextLayers);
              reloadLayers = true;
            }
          }

          ImGui::BeginTable(
            "##Errors",
            3,
            ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_RowBg);
          ImGui::TableSetupColumn(
            "RowNumber", ImGuiTableColumnFlags_WidthFixed);
          ImGui::TableSetupColumn("Description");
          ImGui::TableSetupColumn("Buttons", ImGuiTableColumnFlags_WidthFixed);
          for (size_t i = 0; i < selectedErrors.size(); ++i) {
            const auto& error = selectedErrors.at(i);
            const auto desc = error->GetDescription();

            ImGui::PushID(i);
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::Text("%llu.", i + 1);
            ImGui::TableNextColumn();
            ImGui::TextWrapped("%s", desc.c_str());
            ImGui::TableNextColumn();
            {
              auto fixer = std::dynamic_pointer_cast<FixableLintError>(error);
              ImGui::BeginDisabled(!fixer);
              if (ImGui::Button("Fix It!")) {
                SetAPILayers(fixer->Fix(layers));
                reloadLayers = true;
              }
              ImGui::EndDisabled();
            }
            ImGui::SameLine();
            if (ImGui::Button("Copy")) {
              ImGui::SetClipboardText(desc.c_str());
            }

            ImGui::PopID();
          }
          ImGui::EndTable();
        }
        ImGui::Unindent();

        ImGui::EndChild();
        ImGui::EndTabItem();
      }

      if (ImGui::BeginTabItem("Details")) {
        ImGui::BeginChild("##ScrollArea", {-FLT_MIN, -FLT_MIN});
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
                  ImGui::SetClipboardText(
                    details.mLibraryPath.string().c_str());
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
        ImGui::EndChild();
        ImGui::EndTabItem();
      }

      ImGui::EndTabBar();
    }

    ImGui::End();

    ImGui::SFML::Render(window);
    window.display();
  }
  ImGui::SFML::Shutdown();
}

}// namespace FredEmmott::OpenXRLayers::GUI
