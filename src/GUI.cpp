// Copyright 2023 Fred Emmott <fred@fredemmott.com>
// SPDX-License-Identifier: ISC

#include "GUI.hpp"

#include <fmt/format.h>

#include <ranges>

#include <imgui.h>

#include "APILayer.hpp"
#include "APILayerStore.hpp"
#include "Config.hpp"
#include "Linter.hpp"
#include "Platform.hpp"
#include "SaveReport.hpp"

namespace FredEmmott::OpenXRLayers {

void GUI::DrawFrame() {
  ImGui::Begin(
    "MainWindow",
    nullptr,
    ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove
      | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings
      | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar
      | ImGuiWindowFlags_NoScrollWithMouse);

  if (ImGui::BeginTabBar("##LayerSetTabs", ImGuiTabBarFlags_None)) {
    for (auto&& layerSet: std::views::transform(
           mLayerSets, [](const auto& up) -> auto& { return *up; })) {
      const auto name = layerSet.GetStore().GetDisplayName();
      const auto label = layerSet.HasErrors()
        ? fmt::format("{} {}", Config::GLYPH_ERROR, name)
        : name;
      const auto labelWithID = fmt::format("{}###layerSet-{}", label, name);
      if (ImGui::BeginTabItem(labelWithID.c_str())) {
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

    if (ImGui::TabItemButton("Save Report...", ImGuiTabItemFlags_Trailing)) {
      this->Export();
    }

    ImGui::EndTabBar();
  }

  ImGui::End();
}

GUI::GUI(const ShowExplicit showExplicitMode) {
  const auto stores = ReadWriteAPILayerStore::Get();
  const auto showExplicit
    = showExplicitMode == ShowExplicit::Always
    || std::ranges::any_of(stores, [](const auto& store) {
         if (store->GetKind() != APILayer::Kind::Explicit) {
           return false;
         }
         return !store->GetAPILayers().empty();
       });

  for (auto&& store: stores) {
    if (store->GetKind() == APILayer::Kind::Explicit && !showExplicit) {
      continue;
    }
    mLayerSets.emplace_back(std::make_unique<LayerSet>(store));
  }
}

GUI::~GUI() = default;

void GUI::Run() {
  auto& platform = Platform::Get();
  platform.GUIMain(std::bind_front(&GUI::DrawFrame, this));
}

GUI::LayerSet::~LayerSet() = default;

GUI::LayerSet::LayerSet(APILayerStore* const store)
  : mStore(store),
    mReadWriteStore(dynamic_cast<ReadWriteAPILayerStore*>(store)) {
  mOnChangeConnection
    = store->OnChange([this] { this->mLayerDataIsStale = true; });
  mOnLoaderDataConnection = Platform::Get().OnLoaderData(
    [this] { this->mLintErrorsAreStale = true; });
}

GUI::LayerSet::LayerSet(ReadWriteAPILayerStore* const store)
  : mStore(store),
    mReadWriteStore(store) {
  mOnChangeConnection
    = store->OnChange([this] { this->mLayerDataIsStale = true; });
  mOnLoaderDataConnection = Platform::Get().OnLoaderData(
    [this] { this->mLintErrorsAreStale = true; });
}

void GUI::LayerSet::GUILayersList() {
  auto viewport = ImGui::GetMainViewport();
  const auto dpiScale = Platform::Get().GetDPIScaling();
  if (!ImGui::BeginListBox(
        "##Layers",
        {viewport->WorkSize.x - (256 * dpiScale), viewport->WorkSize.y / 2})) {
    return;
  }
  ImGuiListClipper clipper {};
  clipper.Begin(static_cast<int>(mLayers.size()));

  while (clipper.Step()) {
    for (auto i = clipper.DisplayStart; i < clipper.DisplayEnd; i++) {
      auto& layer = mLayers.at(i);
      auto layerErrors
        = std::ranges::filter_view(mLintErrors, [&layer](const auto& error) {
            return error->GetAffectedLayers().contains(layer);
          });

      ImGui::PushID(i);

      bool layerIsEnabled = layer.IsEnabled();
      ImGui::BeginDisabled(!mReadWriteStore);
      if (ImGui::Checkbox("##Enabled", &layerIsEnabled)) {
        using Value = APILayer::Value;
        layer.mValue = layerIsEnabled ? Value::Enabled : Value::Disabled;
        if (mReadWriteStore->SetAPILayers(mLayers)) {
          mLintErrorsAreStale = true;
          mLayerDataIsStale = true;
        }
      }
      ImGui::EndDisabled();

      auto label = layer.GetKey().mValue;

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
          const auto sourceIndex = *static_cast<const size_t*>(payload->Data);
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

  ImGui::BeginDisabled(!IsReadWrite());
  if (ImGui::Button("Add Layers...", {-FLT_MIN, 0})) {
    this->AddLayersClicked();
  }
  ImGui::EndDisabled();

  ImGui::BeginDisabled(mSelectedLayer == nullptr || !IsReadWrite());
  if (ImGui::Button("Remove Layer...", {-FLT_MIN, 0})) {
    ImGui::OpenPopup("Remove Layer");
  }
  ImGui::EndDisabled();
  if (IsReadWrite()) {
    this->GUIRemoveLayerPopup();
  }

  ImGui::Separator();

  {
    ImGui::BeginDisabled(
      !(mSelectedLayer && !mSelectedLayer->IsEnabled() && mReadWriteStore));
    using Value = APILayer::Value;
    if (ImGui::Button("Enable Layer", {-FLT_MIN, 0})) {
      mSelectedLayer->mValue = Value::Enabled;
      if (mReadWriteStore->SetAPILayers(mLayers)) {
        mLintErrorsAreStale = true;
      }
    }

    if (ImGui::Button("Disable Layer", {-FLT_MIN, 0})) {
      mSelectedLayer->mValue = Value::Disabled;
      if (mReadWriteStore->SetAPILayers(mLayers)) {
        mLintErrorsAreStale = true;
      }
    }
    ImGui::EndDisabled();
  }

  ImGui::Separator();

  ImGui::BeginDisabled(
    !(mSelectedLayer && *mSelectedLayer != mLayers.front() && mReadWriteStore));
  if (ImGui::Button("Move Up", {-FLT_MIN, 0})) {
    auto newLayers = mLayers;
    auto it = std::ranges::find(newLayers, *mSelectedLayer);
    if (it != newLayers.begin() && it != newLayers.end()) {
      std::iter_swap((it - 1), it);
      if (mReadWriteStore->SetAPILayers(newLayers)) {
        mLayerDataIsStale = true;
      }
    }
  }
  ImGui::EndDisabled();
  ImGui::BeginDisabled(
    !(mSelectedLayer && *mSelectedLayer != mLayers.back() && mReadWriteStore));
  if (ImGui::Button("Move Down", {-FLT_MIN, 0})) {
    auto newLayers = mLayers;
    auto it = std::ranges::find(newLayers, *mSelectedLayer);
    if (it != newLayers.end() && (it + 1) != newLayers.end()) {
      std::iter_swap(it, it + 1);
      if (mReadWriteStore->SetAPILayers(newLayers)) {
        mLayerDataIsStale = true;
      }
    }
  }
  ImGui::EndDisabled();

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

    const auto loaderDataPending = std::ranges::any_of(
      mStore->GetArchitectures().enumerate(), [](const auto arch) {
        const auto data = Platform::Get().GetLoaderData(arch);
        if (data) {
          return false;
        }
        return holds_alternative<LoaderData::PendingError>(data.error());
      });
    if (loaderDataPending) {
      ImGui::Text("⌛ Loading...");
      ImGui::Separator();
    }

    if (mSelectedLayer) {
      ImGui::Text("For %s:", mSelectedLayer->mManifestPath.string().c_str());
    } else {
      ImGui::Text("All layers:");
    }

    LintErrors selectedErrors {};
    if (mSelectedLayer) {
      auto view = std::ranges::filter_view(
        mLintErrors, [layer = mSelectedLayer](const auto& error) {
          return error->GetAffectedLayers().contains(*layer);
        });
      selectedErrors = {view.begin(), view.end()};
    } else {
      selectedErrors = mLintErrors;
    }

    ImGui::Indent();

    if (selectedErrors.empty()) {
      ImGui::Separator();
      ImGui::BeginDisabled();
      if (mSelectedLayer) {
        if (mSelectedLayer->IsEnabled()) {
          ImGui::Text("No warnings.");
        } else {
          ImGui::Text(
            "No warnings, however most checks were skipped because the layer "
            "is disabled.");
        }
      } else {
        // No layers selected
        if (std::ranges::any_of(mLayers, &APILayer::IsEnabled)) {
          ImGui::Text("No warnings in enabled layers.");
        } else {
          ImGui::Text(
            "No warnings, however most checks were skipped because there are "
            "no enabled layers.");
        }
      }
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
        if (const auto store = mReadWriteStore) {
          ImGui::SameLine();
          if (ImGui::Button("Fix Them!")) {
            auto nextLayers = mLayers;
            for (auto&& fixable: fixableErrors) {
              nextLayers = fixable->Fix(nextLayers);
            }
            if (store->SetAPILayers(nextLayers)) {
              mLayerDataIsStale = true;
            }
          }
        }
      }

      ImGui::BeginTable(
        "##Errors", 3, ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_RowBg);
      ImGui::TableSetupColumn("RowNumber", ImGuiTableColumnFlags_WidthFixed);
      ImGui::TableSetupColumn("Description");
      ImGui::TableSetupColumn("Buttons", ImGuiTableColumnFlags_WidthFixed);
      for (int i = 0; i < selectedErrors.size(); ++i) {
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
          const auto fixer = std::dynamic_pointer_cast<FixableLintError>(error);
          const auto fixable = fixer && fixer->IsFixable() && mReadWriteStore;
          ImGui::BeginDisabled(!fixable);
          if (ImGui::Button("Fix It!")) {
            if (mReadWriteStore->SetAPILayers(fixer->Fix(mLayers))) {
              mLayerDataIsStale = true;
            }
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
        ImGui::SetClipboardText(mSelectedLayer->mManifestPath.string().c_str());
      }
      ImGui::SameLine();
      ImGui::Text("%s", mSelectedLayer->mManifestPath.string().c_str());

      const APILayerDetails details {mSelectedLayer->mManifestPath};
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
            ImGui::Text("%s", text.c_str());
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

        if (!details.mImplementationVersion.empty()) {
          ImGui::TableNextRow();
          ImGui::TableNextColumn();
          ImGui::Text("Version");
          ImGui::TableNextColumn();
          if (ImGui::Button("Copy##ImplementationVersion")) {
            ImGui::SetClipboardText(details.mImplementationVersion.c_str());
          }
          ImGui::SameLine();
          ImGui::Text("v%s", details.mImplementationVersion.c_str());
        }

        if (!details.mDescription.empty()) {
          ImGui::TableNextRow();
          ImGui::TableNextColumn();
          ImGui::Text("Description");
          ImGui::TableNextColumn();
          ImGui::Text("%s", details.mDescription.c_str());
        }

        if (!details.mAPIVersion.empty()) {
          ImGui::TableNextRow();
          ImGui::TableNextColumn();
          ImGui::Text("OpenXR API Version");
          ImGui::TableNextColumn();
          ImGui::Text("%s", details.mAPIVersion.c_str());
        }

        if (!details.mFileFormatVersion.empty()) {
          ImGui::TableNextRow();
          ImGui::TableNextColumn();
          ImGui::Text("File Format Version");
          ImGui::TableNextColumn();
          ImGui::Text("v%s", details.mFileFormatVersion.c_str());
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

        if (details.mSignature) {
          ImGui::TableNextRow();
          ImGui::TableNextColumn();
          ImGui::Text("Signed by");
          ImGui::TableNextColumn();
          ImGui::Text("%s", details.mSignature->mSignedBy.c_str());

          ImGui::TableNextRow();
          ImGui::TableNextColumn();
          ImGui::Text("Signed at");
          ImGui::TableNextColumn();
          ImGui::Text(
            "%s",
            std::format(
              "{}",
              std::chrono::time_point_cast<std::chrono::seconds>(
                details.mSignature->mSignedAt))
              .c_str());
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

bool GUI::LayerSet::HasErrors() {
  if (mLayerDataIsStale) {
    this->ReloadLayerDataNow();
  }
  if (mLintErrorsAreStale) {
    this->RunAllLintersNow();
  }
  return !mLintErrors.empty();
}

void GUI::LayerSet::RunAllLintersNow() {
  mLintErrors = RunAllLinters(mStore, mLayers);
  mLintErrorsAreStale = false;
}

void GUI::LayerSet::AddLayersClicked() {
  const auto& store = GetReadWriteStore();
  auto paths = Platform::Get().GetNewAPILayerJSONPaths();
  for (auto it = paths.begin(); it != paths.end();) {
    auto existingLayer = std::ranges::find_if(
      mLayers, [it](const auto& layer) { return layer.mManifestPath == *it; });
    if (existingLayer != mLayers.end()) {
      it = paths.erase(it);
      continue;
    }
    ++it;
  }

  if (paths.empty()) {
    return;
  }
  auto nextLayers = mLayers;
  for (const auto& path: paths) {
    nextLayers.emplace_back(mStore, path, APILayer::Value::Enabled);
  }

  bool changed = false;
  do {
    changed = false;
    auto errors = RunAllLinters(mStore, nextLayers);
    const auto nextKeys = std::views::transform(nextLayers, &APILayer::GetKey)
      | std::ranges::to<std::unordered_set>();

    for (const auto& error: errors) {
      auto fixable = std::dynamic_pointer_cast<FixableLintError>(error);
      if (!fixable) {
        continue;
      }

      if (!std::ranges::any_of(
            fixable->GetAffectedLayers(),
            [&nextKeys](const auto& key) { return nextKeys.contains(key); })) {
        continue;
      }

      const auto fixed = fixable->Fix(nextLayers);

      if (fixed.size() <= nextLayers.size()) {
        // Don't just silently fail. Add it with a lint error
        if (store.SetAPILayers(nextLayers)) {
          mLayerDataIsStale = true;
        }
        return;
      }

      if (fixed != nextLayers) {
        nextLayers = fixed;
        changed = true;
      }
    }
  } while (changed);

  if (store.SetAPILayers(nextLayers)) {
    mLayerDataIsStale = true;
  }
}

void GUI::LayerSet::GUIRemoveLayerPopup() {
  const auto& store = GetReadWriteStore();
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
      mSelectedLayer->mManifestPath.string().c_str());
    ImGui::Separator();
    const auto dpiScaling = Platform::Get().GetDPIScaling();
    ImGui::SetCursorPosX((256 + 128) * dpiScaling);
    if (ImGui::Button("Remove", {64 * dpiScaling, 0}) && mSelectedLayer) {
      auto nextLayers = mLayers;
      auto it = std::ranges::find(nextLayers, *mSelectedLayer);
      if (it != nextLayers.end()) {
        nextLayers.erase(it);
        if (store.SetAPILayers(nextLayers)) {
          mLayerDataIsStale = true;
        }
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
  const auto& store = GetReadWriteStore();

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
    ++targetIt;
  }
  newLayers.insert(targetIt, source);

  assert(mLayers.size() == newLayers.size());
  if (store.SetAPILayers(newLayers)) {
    mLayerDataIsStale = true;
  }
}

void GUI::LayerSet::Draw() {
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

void GUI::Export() {
  const auto path = Platform::Get().GetExportFilePath();
  if (!path) {
    return;
  }

  SaveReport(*path);

  if (std::filesystem::exists(*path)) {
    Platform::Get().ShowFolderContainingFile(*path);
  }
}

}// namespace FredEmmott::OpenXRLayers
