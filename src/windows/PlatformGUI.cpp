// Copyright 2023 Fred Emmott <fred@fredemmott.com>
// SPDX-License-Identifier: ISC

#include <Windows.h>

#include <Unknwn.h>

#include <winrt/base.h>

#include <chrono>
#include <format>

#include <ShlObj.h>
#include <ShlObj_core.h>
#include <dwmapi.h>
#include <imgui_freetype.h>
#include <shellapi.h>
#include <shtypes.h>

#include "Config.hpp"
#include "GUI.hpp"
#include "windows/GetKnownFolderPath.hpp"
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
    {
      ShowWindow(handle, SW_HIDE);
      BOOL darkMode = true;
      DwmSetWindowAttribute(
        handle, DWMWA_USE_IMMERSIVE_DARK_MODE, &darkMode, sizeof(darkMode));
      ShowWindow(handle, SW_SHOW);
    }

    // partial workaround for:
    // - https://github.com/SFML/imgui-sfml/issues/206
    // - https://github.com/SFML/imgui-sfml/issues/212
    //
    // remainder is in GUI.cpp
    SetFocus(handle);

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

  std::optional<std::filesystem::path> GetExportFilePath() override {
    auto picker = winrt::create_instance<IFileSaveDialog>(
      CLSID_FileSaveDialog, CLSCTX_ALL);
    {
      FILEOPENDIALOGOPTIONS opts;
      picker->GetOptions(&opts);
      opts |= FOS_FORCEFILESYSTEM | FOS_ALLOWMULTISELECT;
      picker->SetOptions(opts);
    }

    {
      constexpr winrt::guid persistenceGuid {
        0x4e4c8046,
        0x231a,
        0x4ffc,
        {0x82, 0x01, 0x68, 0xe5, 0x17, 0x18, 0x58, 0xb0}};
      picker->SetClientGuid(persistenceGuid);
    }

    picker->SetTitle(L"Export to File");

    winrt::com_ptr<IShellFolder> desktopFolder;
    winrt::check_hresult(SHGetDesktopFolder(desktopFolder.put()));
    winrt::com_ptr<IShellItem> desktopItem;
    winrt::check_hresult(SHGetItemFromObject(
      desktopFolder.get(), IID_PPV_ARGS(desktopItem.put())));
    picker->SetDefaultFolder(desktopItem.get());

    COMDLG_FILTERSPEC filter {
      .pszName = L"Plain Text",
      .pszSpec = L"*.txt",
    };
    picker->SetFileTypes(1, &filter);

    const auto filename = std::format(
      L"OpenXR-API-Layers-{:%Y-%m-%d-%H-%M-%S}.txt",
      std::chrono::zoned_time(
        std::chrono::current_zone(),
        std::chrono::time_point_cast<std::chrono::seconds>(
          std::chrono::system_clock::now())));
    picker->SetFileName(filename.c_str());

    if (picker->Show(NULL) != S_OK) {
      return {};
    }

    winrt::com_ptr<IShellItem> shellItem;
    if (picker->GetResult(shellItem.put()) != S_OK) {
      return {};
    }

    PWSTR buf {nullptr};
    winrt::check_hresult(shellItem->GetDisplayName(SIGDN_FILESYSPATH, &buf));
    const std::filesystem::path ret {std::wstring_view {buf}};
    CoTaskMemFree(buf);

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
    const auto fontsPath = GetKnownFolderPath<FOLDERID_Fonts>();

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

  float GetDPIScaling() override {
    return mDPIScaling;
  }

  void ShowFolderContainingFile(const std::filesystem::path& path) override {
    const auto absolute = std::filesystem::absolute(path).wstring();

    PIDLIST_ABSOLUTE pidl {nullptr};
    winrt::check_hresult(SHParseDisplayName(
      std::filesystem::absolute(path).wstring().c_str(),
      nullptr,
      &pidl,
      0,
      nullptr));

    SHOpenFolderAndSelectItems(pidl, 0, nullptr, 0);

    CoTaskMemFree(pidl);
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