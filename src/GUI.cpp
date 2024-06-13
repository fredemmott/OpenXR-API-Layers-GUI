// Copyright 2023 Fred Emmott <fred@fredemmott.com>
// SPDX-License-Identifier: ISC

#include "GUI.hpp"

#include <SFML/Graphics/RenderWindow.hpp>
#include <SFML/System/Clock.hpp>
#include <SFML/Window/Event.hpp>
#include <fmt/format.h>

#include <ranges>
#include <unordered_map>

#include <imgui.h>

#include "APILayer.hpp"
#include "APILayerStore.hpp"
#include "Config.hpp"
#include "Linter.hpp"
#include "SaveReport.hpp"
#include <imgui-SFML.h>

namespace FredEmmott::OpenXRLayers {

namespace {
constexpr ImVec2 MINIMUM_WINDOW_SIZE {1024, 768};

class MyWindow final : public sf::RenderWindow {
 public:
  using sf::RenderWindow::RenderWindow;
  virtual ~MyWindow() = default;

  void setMinimumSize(const ImVec2& value) {
    mMinimumSize = value;
    this->onResize();
  }

 protected:
  virtual void onResize() override {
    auto size = this->getSize();
    auto newSize = size;
    if (size.x < mMinimumSize.x) {
      newSize.x = static_cast<unsigned int>(mMinimumSize.x);
    }
    if (size.y < mMinimumSize.y) {
      newSize.y = static_cast<unsigned int>(mMinimumSize.y);
    }

    if (size == newSize) {
      return;
    }

    this->setSize(newSize);
  }

 private:
  ImVec2 mMinimumSize {MINIMUM_WINDOW_SIZE};
};
}// namespace

void GUI::Run() {
  auto& platform = PlatformGUI::Get();

  MyWindow window {
    sf::VideoMode(MINIMUM_WINDOW_SIZE.x, MINIMUM_WINDOW_SIZE.y),
    fmt::format("OpenXR API Layers v{}", Config::BUILD_VERSION)};
  window.setFramerateLimit(60);
  if (!ImGui::SFML::Init(window)) {
    return;
  }
  mWindowHandle = window.getSystemHandle();
  platform.SetWindow(mWindowHandle);

  auto dpiScaling = platform.GetDPIScaling();
  window.setMinimumSize({
    MINIMUM_WINDOW_SIZE.x * dpiScaling,
    MINIMUM_WINDOW_SIZE.y * dpiScaling,
  });
  platform.SetupFonts(&ImGui::GetIO());

  sf::Clock deltaClock {};

  std::vector<LayerSet> layerSets;
  for (auto&& store: ReadWriteAPILayerStore::Get()) {
    layerSets.push_back({std::move(store)});
  }
  while (window.isOpen()) {
    {
      const auto changeInfo = platform.GetDPIChangeInfo();
      if (changeInfo) {
        window.setMinimumSize({0, 0});
        if (changeInfo->mRecommendedSize) {
          window.setSize(*changeInfo->mRecommendedSize);
        }
        window.setMinimumSize({
          MINIMUM_WINDOW_SIZE.x * changeInfo->mDPIScaling,
          MINIMUM_WINDOW_SIZE.y * changeInfo->mDPIScaling,
        });
        platform.SetupFonts(&ImGui::GetIO());
      }
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

    if (ImGui::BeginTabBar("##LayerSetTabs", ImGuiTabBarFlags_None)) {
      for (auto& layerSet: layerSets) {
        if (ImGui::BeginTabItem(layerSet.mStore->GetDisplayName().c_str())) {
          layerSet.Draw();
          ImGui::EndTabItem();
        }
      }

      if (ImGui::BeginTabItem("About")) {
        ImGui::TextWrapped(
          "OpenXR API Layers GUI v%s\n\n---\n\n%s",
          Config::BUILD_VERSION,
          Config::LICENSE_TEXT);
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

void GUI::LayerSet::GUILayersList() {
  auto viewport = ImGui::GetMainViewport();
  const auto dpiScale = PlatformGUI::Get().GetDPIScaling();
  ImGui::BeginListBox(
    "##Layers",
    {viewport->WorkSize.x - (256 * dpiScale), viewport->WorkSize.y / 2});
  ImGuiListClipper clipper {};
  clipper.Begin(static_cast<int>(mLayers.size()));

  while (clipper.Step()) {
    for (auto i = clipper.DisplayStart; i < clipper.DisplayEnd; i++) {
      auto& layer = mLayers.at(i);
      auto layerErrors
        = std::ranges::filter_view(mLintErrors, [&layer](const auto& error) {
            return error->GetAffectedLayers().contains(layer.mJSONPath);
          });

      ImGui::PushID(i);

      bool layerIsEnabled = layer.IsEnabled();
      if (ImGui::Checkbox("##Enabled", &layerIsEnabled)) {
        using Value = APILayer::Value;
        layer.mValue = layerIsEnabled ? Value::Enabled : Value::Disabled;
        mLintErrorsAreStale = true;
        mStore->SetAPILayers(mLayers);
      }

      auto label = layer.mJSONPath.string();

      if (!layerErrors.empty()) {
        label = fmt::format("{} {}", Config::GLYPH_ERROR, label);
      }

      ImGui::SameLine();

      if (ImGui::Selectable(label.c_str(), mSelectedLayer == &layer)) {
        mSelectedLayer = &layer;
      }

      if (ImGui::BeginDragDropSource()) {
        const size_t index = i;
        ImGui::SetDragDropPayload("APILayerIndex", &index, sizeof(index));
        ImGui::EndDragDropSource();
      }

      if (ImGui::BeginDragDropTarget()) {
        if (
          const auto payload = ImGui::AcceptDragDropPayload("APILayerIndex")) {
          auto sourceIndex = *reinterpret_cast<const size_t*>(payload->Data);
          const auto& source = mLayers.at(sourceIndex);
          DragDropReorder(source, layer);
        }
        ImGui::EndDragDropTarget();
      }

      ImGui::PopID();
    }
  }
  ImGui::EndListBox();
}

void GUI::LayerSet::GUIButtons() {
  ImGui::BeginGroup();
  if (ImGui::Button("Reload List", {-FLT_MIN, 0})) {
    mLayerDataIsStale = true;
  }

  if (ImGui::Button("Add Layers...", {-FLT_MIN, 0})) {
    this->AddLayersClicked();
  }

  ImGui::BeginDisabled(mSelectedLayer == nullptr);
  if (ImGui::Button("Remove Layer...", {-FLT_MIN, 0})) {
    ImGui::OpenPopup("Remove Layer");
  }
  ImGui::EndDisabled();
  this->GUIRemoveLayerPopup();

  ImGui::Separator();

  ImGui::BeginDisabled(!(mSelectedLayer && !mSelectedLayer->IsEnabled()));
  using Value = APILayer::Value;
  if (ImGui::Button("Enable Layer", {-FLT_MIN, 0})) {
    mSelectedLayer->mValue = Value::Enabled;
    mLintErrorsAreStale = true;
    mStore->SetAPILayers(mLayers);
  }
  ImGui::EndDisabled();

  ImGui::BeginDisabled(!(mSelectedLayer && mSelectedLayer->IsEnabled()));
  if (ImGui::Button("Disable Layer", {-FLT_MIN, 0})) {
    mSelectedLayer->mValue = Value::Disabled;
    mLintErrorsAreStale = true;
    mStore->SetAPILayers(mLayers);
  }
  ImGui::EndDisabled();

  ImGui::Separator();

  ImGui::BeginDisabled(!(mSelectedLayer && *mSelectedLayer != mLayers.front()));
  if (ImGui::Button("Move Up", {-FLT_MIN, 0})) {
    auto newLayers = mLayers;
    auto it = std::ranges::find(newLayers, *mSelectedLayer);
    if (it != newLayers.begin() && it != newLayers.end()) {
      std::iter_swap((it - 1), it);
      mStore->SetAPILayers(newLayers);
      mLayerDataIsStale = true;
    }
  }
  ImGui::EndDisabled();
  ImGui::BeginDisabled(!(mSelectedLayer && *mSelectedLayer != mLayers.back()));
  if (ImGui::Button("Move Down", {-FLT_MIN, 0})) {
    auto newLayers = mLayers;
    auto it = std::ranges::find(newLayers, *mSelectedLayer);
    if (it != newLayers.end() && (it + 1) != newLayers.end()) {
      std::iter_swap(it, it + 1);
      mStore->SetAPILayers(newLayers);
      mLayerDataIsStale = true;
    }
  }
  ImGui::EndDisabled();

  ImGui::Spacing();
  if (ImGui::Button("Export...", {-FLT_MIN, 0})) {
    this->Export();
  }

  ImGui::EndGroup();
}

void GUI::LayerSet::GUITabs() {
  if (ImGui::BeginTabBar("##ErrorDetailsTabs", ImGuiTabBarFlags_None)) {
    this->GUIErrorsTab();
    this->GUIDetailsTab();

    ImGui::EndTabBar();
  }
}

void GUI::LayerSet::GUIErrorsTab() {
  if (ImGui::BeginTabItem("Warnings")) {
    ImGui::BeginChild("##ScrollArea", {-FLT_MIN, -FLT_MIN});
    if (mSelectedLayer) {
      ImGui::Text("For %s:", mSelectedLayer->mJSONPath.string().c_str());
    } else {
      ImGui::Text("All layers:");
    }

    LintErrors selectedErrors {};
    if (mSelectedLayer) {
      auto view = std::ranges::filter_view(
        mLintErrors, [layer = mSelectedLayer](const auto& error) {
          return error->GetAffectedLayers().contains(layer->mJSONPath);
        });
      selectedErrors = {view.begin(), view.end()};
    } else {
      selectedErrors = mLintErrors;
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
            "%s",
            fmt::format(
              "All {} warnings are automatically fixable:",
              fixableErrors.size())
              .c_str());
        } else {
          ImGui::Text(
            "%s",
            fmt::format(
              "{} out of {} warnings are automatically "
              "fixable:",
              fixableErrors.size(),
              selectedErrors.size())
              .c_str());
        }
        ImGui::SameLine();
        if (ImGui::Button("Fix Them!")) {
          auto nextLayers = mLayers;
          for (auto& fixable: fixableErrors) {
            nextLayers = fixable->Fix(nextLayers);
          }
          mStore->SetAPILayers(nextLayers);
          mLayerDataIsStale = true;
        }
      }

      ImGui::BeginTable(
        "##Errors", 3, ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_RowBg);
      ImGui::TableSetupColumn("RowNumber", ImGuiTableColumnFlags_WidthFixed);
      ImGui::TableSetupColumn("Description");
      ImGui::TableSetupColumn("Buttons", ImGuiTableColumnFlags_WidthFixed);
      for (size_t i = 0; i < selectedErrors.size(); ++i) {
        const auto& error = selectedErrors.at(i);
        const auto desc = error->GetDescription();

        ImGui::PushID(i);
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::Text("%s", fmt::to_string(i + 1).c_str());
        ImGui::TableNextColumn();
        ImGui::TextWrapped("%s", desc.c_str());
        ImGui::TableNextColumn();
        {
          auto fixer = std::dynamic_pointer_cast<FixableLintError>(error);
          ImGui::BeginDisabled(!fixer);
          if (ImGui::Button("Fix It!")) {
            mStore->SetAPILayers(fixer->Fix(mLayers));
            mLayerDataIsStale = true;
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
}

void GUI::LayerSet::GUIDetailsTab() {
  if (ImGui::BeginTabItem("Details")) {
    ImGui::BeginChild("##ScrollArea", {-FLT_MIN, -FLT_MIN});
    if (mSelectedLayer) {
      ImGui::BeginTable(
        "##DetailsTable",
        2,
        ImGuiTableFlags_Borders | ImGuiTableFlags_SizingFixedFit);

      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      ImGui::Text("JSON File");
      ImGui::TableNextColumn();
      if (ImGui::Button("Copy##CopyJSONFile")) {
        ImGui::SetClipboardText(mSelectedLayer->mJSONPath.string().c_str());
      }
      ImGui::SameLine();
      ImGui::Text("%s", mSelectedLayer->mJSONPath.string().c_str());

      const APILayerDetails details {mSelectedLayer->mJSONPath};
      if (details.mState != APILayerDetails::State::Loaded) {
        const auto error = details.StateAsString();
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
              "%s", fmt::format("{} [none]", Config::GLYPH_ERROR).c_str());
          } else {
            auto text = details.mLibraryPath.string();
            if (!std::filesystem::exists(details.mLibraryPath)) {
              text = fmt::format("{} {}", Config::GLYPH_ERROR, text);
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
            ImGui::PushID(ext.mName.c_str());
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            if (ImGui::Button("Copy")) {
              ImGui::SetClipboardText(ext.mName.c_str());
            }
            ImGui::SameLine();
            ImGui::AlignTextToFramePadding();
            ImGui::Text("%s", ext.mName.c_str());
            ImGui::TableNextColumn();
            ImGui::Text("%s", ext.mVersion.c_str());
            ImGui::PopID();
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
}

void GUI::LayerSet::ReloadLayerDataNow() {
  auto newLayers = mStore->GetAPILayers();
  if (mSelectedLayer) {
    auto it = std::ranges::find(newLayers, *mSelectedLayer);
    if (it != newLayers.end()) {
      mSelectedLayer = &*it;
    } else {
      mSelectedLayer = nullptr;
    }
  }
  mLayers = std::move(newLayers);
  mLayerDataIsStale = false;
  mLintErrorsAreStale = true;
}

void GUI::LayerSet::RunAllLintersNow() {
  std::unordered_map<std::filesystem::path, APILayer*> layersByPath;
  for (auto& layer: mLayers) {
    layersByPath.emplace(layer.mJSONPath, &layer);
  }
  mLintErrors = RunAllLinters(mStore, mLayers);
  mLintErrorsAreStale = false;
}

void GUI::LayerSet::AddLayersClicked() {
  auto paths = PlatformGUI::Get().GetNewAPILayerJSONPaths();
  for (auto it = paths.begin(); it != paths.end();) {
    auto existingLayer = std::ranges::find_if(
      mLayers, [it](const auto& layer) { return layer.mJSONPath == *it; });
    if (existingLayer != mLayers.end()) {
      it = paths.erase(it);
      continue;
    }
    it++;
  }

  if (paths.empty()) {
    return;
  }
  auto nextLayers = mLayers;
  for (const auto& path: paths) {
    nextLayers.push_back(APILayer {
      .mJSONPath = path,
      .mValue = APILayer::Value::Enabled,
    });
  }

  bool changed = false;
  do {
    changed = false;
    auto errors = RunAllLinters(mStore, nextLayers);
    for (const auto& error: errors) {
      auto fixable = std::dynamic_pointer_cast<FixableLintError>(error);
      if (!fixable) {
        continue;
      }

      if (!std::ranges::any_of(
            fixable->GetAffectedLayers(), [&paths](const auto& it) {
              return std::ranges::find(paths, it) != paths.end();
            })) {
        continue;
      }

      const auto fixed = fixable->Fix(nextLayers);
      if (fixed != nextLayers) {
        nextLayers = fixed;
        changed = true;
      }
    }
  } while (changed);

  mStore->SetAPILayers(nextLayers);
  mLayerDataIsStale = true;
}

void GUI::LayerSet::GUIRemoveLayerPopup() {
  auto viewport = ImGui::GetMainViewport();
  ImVec2 center = viewport->GetCenter();
  ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
  ImGui::SetNextWindowSize({viewport->WorkSize.x / 2, 0}, ImGuiCond_Appearing);
  if (ImGui::BeginPopupModal(
        "Remove Layer", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::TextWrapped(
      "Are you sure you want to completely remove '%s'?\n\nThis can not "
      "be "
      "undone.",
      mSelectedLayer->mJSONPath.string().c_str());
    ImGui::Separator();
    const auto dpiScaling = PlatformGUI::Get().GetDPIScaling();
    ImGui::SetCursorPosX((256 + 128) * dpiScaling);
    if (ImGui::Button("Remove", {64 * dpiScaling, 0}) && mSelectedLayer) {
      auto nextLayers = mLayers;
      auto it = std::ranges::find(nextLayers, *mSelectedLayer);
      if (it != nextLayers.end()) {
        nextLayers.erase(it);
        mStore->SetAPILayers(nextLayers);
        mLayerDataIsStale = true;
      }
      ImGui::CloseCurrentPopup();
    }
    ImGui::SameLine();
    if (ImGui::Button("Cancel", {64 * dpiScaling, 0})) {
      ImGui::CloseCurrentPopup();
    }
    ImGui::SetItemDefaultFocus();

    ImGui::EndPopup();
  }
}

void GUI::LayerSet::DragDropReorder(
  const APILayer& source,
  const APILayer& target) {
  auto newLayers = mLayers;

  auto sourceIt = std::ranges::find(newLayers, source);
  if (sourceIt == newLayers.end()) {
    return;
  }

  auto targetIt = std::ranges::find(newLayers, target);
  if (targetIt == newLayers.end() || sourceIt == targetIt) {
    return;
  }

  const auto insertBefore = sourceIt > targetIt;

  newLayers.erase(sourceIt);
  targetIt = std::ranges::find(newLayers, target);
  assert(targetIt != newLayers.end());
  if (!insertBefore) {
    targetIt++;
  }
  newLayers.insert(targetIt, source);

  assert(mLayers.size() == newLayers.size());
  if (mStore->SetAPILayers(newLayers)) {
    mLayerDataIsStale = true;
  }
}

void GUI::LayerSet::Draw() {
  if (mStore->Poll()) {
    mLayerDataIsStale = true;
  }
  if (mLayerDataIsStale) {
    this->ReloadLayerDataNow();
  }

  if (mLintErrorsAreStale) {
    this->RunAllLintersNow();
  }

  this->GUILayersList();
  ImGui::SameLine();
  this->GUIButtons();

  ImGui::SetNextItemWidth(-FLT_MIN);
  this->GUITabs();
}

void GUI::LayerSet::Export() {
  const auto path = PlatformGUI::Get().GetExportFilePath();
  if (!path) {
    return;
  }

  SaveReport(*path);

  if (std::filesystem::exists(*path)) {
    PlatformGUI::Get().ShowFolderContainingFile(*path);
  }
}

}// namespace FredEmmott::OpenXRLayers
