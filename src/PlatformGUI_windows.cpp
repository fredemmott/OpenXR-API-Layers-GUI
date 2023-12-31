// Copyright 2023 Fred Emmott <fred@fredemmott.com>
// SPDX-License-Identifier: ISC

#include <Windows.h>

#include <winrt/base.h>

#include <ShlObj.h>
#include <ShlObj_core.h>
#include <imgui_freetype.h>
#include <shellapi.h>

#include "GUI.hpp"
#include <imgui-SFML.h>

namespace FredEmmott::OpenXRLayers {

class PlatformGUI_Windows final : public PlatformGUI {
 private:
  HWND mWindow {};
  float mDPIScaling {};
  std::optional<DPIChangeInfo> mDPIChangeInfo;

 public:
  PlatformGUI_Windows() {
    winrt::init_apartment();
  }

  void SetWindow(sf::WindowHandle handle) override {
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    mWindow = handle;
    mDPIScaling
      = static_cast<float>(GetDpiForWindow(handle)) / USER_DEFAULT_SCREEN_DPI;
    SetWindowSubclass(
      mWindow,
      static_cast<SUBCLASSPROC>(SubclassProc),
      0,
      reinterpret_cast<DWORD_PTR>(this));
    auto icon = LoadIconA(GetModuleHandle(nullptr), "appIcon");
    if (icon) {
      SetClassLongPtr(mWindow, GCLP_HICON, reinterpret_cast<LONG_PTR>(icon));
    }
  }

  std::optional<DPIChangeInfo> GetDPIChangeInfo() override {
    auto ret = mDPIChangeInfo;
    mDPIChangeInfo = {};
    return ret;
  }

  std::vector<std::filesystem::path> GetNewAPILayerJSONPaths() override {
    auto picker = winrt::create_instance<IFileOpenDialog>(
      CLSID_FileOpenDialog, CLSCTX_ALL);

    {
      FILEOPENDIALOGOPTIONS opts;
      picker->GetOptions(&opts);
      opts |= FOS_FORCEFILESYSTEM | FOS_FILEMUSTEXIST | FOS_PATHMUSTEXIST
        | FOS_ALLOWMULTISELECT;
      picker->SetOptions(opts);
    }

    {
      constexpr winrt::guid persistenceGuid {
        0xac83d7a5,
        0xca3a,
        0x45d3,
        {0x90, 0xc2, 0x63, 0xf1, 0x4b, 0x79, 0x24, 0x44}};
      picker->SetClientGuid(persistenceGuid);
    }

    picker->SetTitle(L"Add API Layers");

    // Keep alive until we're done
    COMDLG_FILTERSPEC filter {
      .pszName = L"JSON files",
      .pszSpec = L"*.json",
    };
    picker->SetFileTypes(1, &filter);

    if (picker->Show(NULL) != S_OK) {
      return {};
    }

    winrt::com_ptr<IShellItemArray> items;
    winrt::check_hresult(picker->GetResults(items.put()));

    if (!items) {
      return {};
    }

    DWORD count {};
    winrt::check_hresult(items->GetCount(&count));

    std::vector<std::filesystem::path> ret;

    for (DWORD i = 0; i < count; ++i) {
      winrt::com_ptr<IShellItem> item;
      winrt::check_hresult(items->GetItemAt(i, item.put()));
      wchar_t* buf {nullptr};
      winrt::check_hresult(item->GetDisplayName(SIGDN_FILESYSPATH, &buf));
      if (!buf) {
        continue;
      }

      ret.push_back({std::wstring_view {buf}});
      CoTaskMemFree(buf);
    }
    return ret;
  }

  void SetupFonts(ImGuiIO* io) override {
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
      (fontsPath / "segoeui.ttf").string().c_str(), 16.0f * mDPIScaling);

    static ImWchar ranges[] = {0x1, 0x1ffff, 0};
    static ImFontConfig config {};
    config.OversampleH = config.OversampleV = 1;
    config.MergeMode = true;
    config.FontBuilderFlags |= ImGuiFreeTypeBuilderFlags_LoadColor;

    io->Fonts->AddFontFromFileTTF(
      (fontsPath / "seguiemj.ttf").string().c_str(),
      13.0f * mDPIScaling,
      &config,
      ranges);

    { [[maybe_unused]] auto ignored = ImGui::SFML::UpdateFontTexture(); }
  }

  void OpenURI(const std::string& uri) override {
    auto wstring = std::wstring(winrt::to_hstring(uri));
    ShellExecuteW(NULL, L"open", wstring.c_str(), nullptr, nullptr, 0);
  }

  float GetDPIScaling() override {
    return mDPIScaling;
  }

 protected:
  static LRESULT CALLBACK SubclassProc(
    HWND hWnd,
    UINT uMsg,
    WPARAM wParam,
    LPARAM lParam,
    UINT_PTR uIdSubclass,
    DWORD_PTR dwRefData) {
    return reinterpret_cast<PlatformGUI_Windows*>(dwRefData)->SubclassProc(
      hWnd, uMsg, wParam, lParam, uIdSubclass);
  }

  LRESULT SubclassProc(
    HWND hWnd,
    UINT uMsg,
    WPARAM wParam,
    LPARAM lParam,
    UINT_PTR uIdSubclass) {
    if (uMsg == WM_DPICHANGED) {
      mDPIScaling
        = static_cast<float>(HIWORD(wParam)) / USER_DEFAULT_SCREEN_DPI;
      auto rect = reinterpret_cast<RECT*>(lParam);
      mDPIChangeInfo = DPIChangeInfo {
        .mDPIScaling = mDPIScaling,
        .mRecommendedSize = ImVec2 {
          static_cast<float>(rect->right - rect->left),
          static_cast<float>(rect->bottom - rect->top),
        },
      };
    }
    return DefSubclassProc(hWnd, uMsg, wParam, lParam);
  }
};

PlatformGUI& PlatformGUI::Get() {
  static PlatformGUI_Windows sInstance {};
  return sInstance;
}

}// namespace FredEmmott::OpenXRLayers