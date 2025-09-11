// Copyright 2023 Fred Emmott <fred@fredemmott.com>
// SPDX-License-Identifier: ISC

#include <Unknwn.h>
#include <Windows.h>

#include <winrt/base.h>

#include <wil/com.h>
#include <wil/resource.h>

#include <nlohmann/json.hpp>

#include <bit>
#include <chrono>
#include <format>

#include <ShlObj.h>
#include <ShlObj_core.h>
#include <d3d11.h>
#include <dwmapi.h>
#include <dxgi1_3.h>
#include <imgui_freetype.h>
#include <imgui_impl_dx11.h>
#include <imgui_impl_win32.h>
#include <shellapi.h>
#include <shtypes.h>

#include "CheckForUpdates.hpp"
#include "Config.hpp"
#include "GUI.hpp"
#include "LoaderData.hpp"
#include "windows/GetKnownFolderPath.hpp"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(
  HWND hWnd,
  UINT msg,
  WPARAM wParam,
  LPARAM lParam);

namespace FredEmmott::OpenXRLayers {

class PlatformGUI_Windows final : public PlatformGUI {
 public:
  void Run(std::function<void()> drawFrame) override;

  std::optional<std::filesystem::path> GetExportFilePath() override;
  std::vector<std::filesystem::path> GetNewAPILayerJSONPaths() override;
  std::expected<LoaderData, std::string> GetLoaderData() override;

  float GetDPIScaling() override {
    return mDPIScaling;
  }
  void ShowFolderContainingFile(const std::filesystem::path& path) override;

 private:
  wil::unique_hwnd mWindowHandle {};
  float mDPIScaling {};
  std::optional<DPIChangeInfo> mDPIChangeInfo;
  AutoUpdateProcess mUpdater {CheckForUpdates()};

  wil::com_ptr<ID3D11Device> mD3DDevice;
  wil::com_ptr<ID3D11DeviceContext> mD3DContext;
  wil::com_ptr<IDXGISwapChain1> mSwapChain;
  wil::com_ptr<ID3D11Texture2D> mBackBuffer;
  wil::com_ptr<ID3D11RenderTargetView> mRenderTargetView;
  bool mWindowClosed = false;

  size_t mFrameCounter = 0;
  ImVec2 mWindowSize {
    Config::MINIMUM_WINDOW_WIDTH,
    Config::MINIMUM_WINDOW_HEIGHT,
  };

  HWND CreateAppWindow();
  void InitializeFonts(ImGuiIO* io);
  void InitializeDirect3D();

  void BeforeFrame();
  void AfterFrame();

  void Initialize();
  void MainLoop(const std::function<void()>& drawFrame);
  void Shutdown();

  static LRESULT WindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

  LRESULT
  InstanceWindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
};

void PlatformGUI_Windows::InitializeDirect3D() {
  const auto hwnd = mWindowHandle.get();
  UINT d3dFlags = D3D11_CREATE_DEVICE_SINGLETHREADED;
  UINT dxgiFlags = 0;
#ifndef NDEBUG
  d3dFlags |= D3D11_CREATE_DEVICE_DEBUG;
  dxgiFlags |= DXGI_CREATE_FACTORY_DEBUG;
#endif
  CheckHRESULT(D3D11CreateDevice(
    nullptr,
    D3D_DRIVER_TYPE_HARDWARE,
    nullptr,
    d3dFlags,
    nullptr,
    0,
    D3D11_SDK_VERSION,
    mD3DDevice.put(),
    nullptr,
    mD3DContext.put()));

  wil::com_ptr<IDXGIFactory3> dxgiFactory;
  CheckHRESULT(CreateDXGIFactory2(dxgiFlags, IID_PPV_ARGS(dxgiFactory.put())));
  constexpr DXGI_SWAP_CHAIN_DESC1 SwapChainDesc {
    .Width = Config::MINIMUM_WINDOW_WIDTH,
    .Height = Config::MINIMUM_WINDOW_HEIGHT,
    .Format = DXGI_FORMAT_R8G8B8A8_UNORM,
    .SampleDesc = {1, 0},
    .BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT,
    .BufferCount = 3,
    .SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD,
    .AlphaMode = DXGI_ALPHA_MODE_IGNORE,
  };
  CheckHRESULT(dxgiFactory->CreateSwapChainForHwnd(
    mD3DDevice.get(),
    hwnd,
    &SwapChainDesc,
    nullptr,
    nullptr,
    mSwapChain.put()));
  CheckHRESULT(mSwapChain->GetBuffer(0, IID_PPV_ARGS(mBackBuffer.put())));
  CheckHRESULT(mD3DDevice->CreateRenderTargetView(
    mBackBuffer.get(), nullptr, mRenderTargetView.put()));
}

void PlatformGUI_Windows::Run(const std::function<void()> drawFrame) {
  this->Initialize();
  const auto shutdown = wil::scope_exit([this] { this->Shutdown(); });
  this->MainLoop(drawFrame);
}

void PlatformGUI_Windows::MainLoop(const std::function<void()>& drawFrame) {
  constexpr auto Interval
    = std::chrono::microseconds(1000000 / Config::MAX_FPS);
  while (true) {
    const auto earliestNextFrame = std::chrono::steady_clock::now() + Interval;
    MSG msg {};
    while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
      TranslateMessage(&msg);
      DispatchMessageW(&msg);
    }

    if (mWindowClosed) {
      return;
    }

    this->BeforeFrame();
    drawFrame();
    this->AfterFrame();

    WaitMessage();
    const auto sleepFor = earliestNextFrame - std::chrono::steady_clock::now();
    if (sleepFor > std::chrono::steady_clock::duration::zero()) {
      std::this_thread::sleep_for(
        std::chrono::duration_cast<std::chrono::milliseconds>(sleepFor));
    }
  }
}

void PlatformGUI_Windows::Initialize() {
  static std::string sIniPath;
  sIniPath = (GetKnownFolderPath<FOLDERID_LocalAppData>()
              / "OpenXR API Layers GUI" / "imgui.ini")
               .string();

  SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
  CheckHRESULT(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED));

  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGui::StyleColorsDark();

  auto& io = ImGui::GetIO();
  io.IniFilename = sIniPath.c_str();
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

  const auto hwnd = this->CreateAppWindow();
  if (!hwnd) {
    return;
  }
  this->InitializeDirect3D();

  ShowWindow(hwnd, SW_SHOW | SW_NORMAL);
  ImGui_ImplWin32_Init(mWindowHandle.get());
  ImGui_ImplDX11_Init(mD3DDevice.get(), mD3DContext.get());
  this->InitializeFonts(&io);
}

void PlatformGUI_Windows::Shutdown() {
  ImGui_ImplDX11_Shutdown();
  ImGui_ImplWin32_Shutdown();
  ImGui::DestroyContext();
}

void PlatformGUI_Windows::BeforeFrame() {
  if ((++mFrameCounter % 60) == 0) {
    mUpdater.ActivateWindowIfVisible();
  }

  const auto rtv = mRenderTargetView.get();
  FLOAT clearColor[4] {0.f, 0.f, 0.f, 1.f};
  mD3DContext->ClearRenderTargetView(rtv, clearColor);
  mD3DContext->OMSetRenderTargets(1, &rtv, nullptr);

  ImGui_ImplDX11_NewFrame();
  ImGui_ImplWin32_NewFrame();
  ImGui::NewFrame();

  ImGui::SetNextWindowPos({0, 0});
  ImGui::SetNextWindowSize(mWindowSize);
}

void PlatformGUI_Windows::AfterFrame() {
  ImGui::Render();
  ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
  CheckHRESULT(mSwapChain->Present(0, 0));
}

void PlatformGUI_Windows::ShowFolderContainingFile(
  const std::filesystem::path& path) {
  const auto absolute = std::filesystem::absolute(path).wstring();

  wil::unique_any<PIDLIST_ABSOLUTE, decltype(&CoTaskMemFree), &CoTaskMemFree>
    pidl;

  CheckHRESULT(SHParseDisplayName(
    std::filesystem::absolute(path).wstring().c_str(),
    nullptr,
    pidl.put(),
    0,
    nullptr));

  SHOpenFolderAndSelectItems(pidl.get(), 0, nullptr, 0);
}

HWND PlatformGUI_Windows::CreateAppWindow() {
  static const auto WindowTitle
    = std::format(L"OpenXR API Layers v{}", Config::BUILD_VERSION_W);
  static const WNDCLASSEXW WindowClass {
    .cbSize = sizeof(WNDCLASSEXW),
    .lpfnWndProc = &PlatformGUI_Windows::WindowProc,
    .hInstance = GetModuleHandle(nullptr),
    .hIcon = LoadIconW(GetModuleHandle(nullptr), L"appIcon"),
    .hCursor = LoadCursorW(nullptr, IDC_ARROW),
    .lpszClassName = L"OpenXR-API-Layers-GUI",
    .hIconSm = LoadIconW(GetModuleHandle(nullptr), L"appIcon"),
  };

  RegisterClassExW(&WindowClass);
  mWindowHandle.reset(CreateWindowExW(
    WS_EX_APPWINDOW | WS_EX_CLIENTEDGE,
    WindowClass.lpszClassName,
    WindowTitle.c_str(),
    WS_OVERLAPPEDWINDOW & (~WS_MAXIMIZEBOX),
    CW_USEDEFAULT,
    CW_USEDEFAULT,
    Config::MINIMUM_WINDOW_WIDTH,
    Config::MINIMUM_WINDOW_HEIGHT,
    nullptr,
    nullptr,
    WindowClass.hInstance,
    this));
  if (!mWindowHandle) {
    return nullptr;
  }
  const auto handle = mWindowHandle.get();

  {
    BOOL darkMode = true;
    DwmSetWindowAttribute(
      handle, DWMWA_USE_IMMERSIVE_DARK_MODE, &darkMode, sizeof(darkMode));
    DWM_SYSTEMBACKDROP_TYPE backdropType {DWMSBT_MAINWINDOW};
    DwmSetWindowAttribute(
      handle, DWMWA_SYSTEMBACKDROP_TYPE, &backdropType, sizeof(backdropType));
  }

  mDPIScaling
    = static_cast<float>(GetDpiForWindow(handle)) / USER_DEFAULT_SCREEN_DPI;

  return handle;
}

std::optional<std::filesystem::path> PlatformGUI_Windows::GetExportFilePath() {
  const auto picker
    = wil::CoCreateInstance<IFileSaveDialog>(CLSID_FileSaveDialog);
  {
    FILEOPENDIALOGOPTIONS opts;
    picker->GetOptions(&opts);
    opts |= FOS_FORCEFILESYSTEM | FOS_ALLOWMULTISELECT;
    picker->SetOptions(opts);
  }

  {
    constexpr GUID persistenceGuid {
      0x4e4c8046,
      0x231a,
      0x4ffc,
      {0x82, 0x01, 0x68, 0xe5, 0x17, 0x18, 0x58, 0xb0}};
    picker->SetClientGuid(persistenceGuid);
  }

  picker->SetTitle(L"Export to File");

  wil::com_ptr<IShellFolder> desktopFolder;
  CheckHRESULT(SHGetDesktopFolder(desktopFolder.put()));
  wil::com_ptr<IShellItem> desktopItem;
  CheckHRESULT(
    SHGetItemFromObject(desktopFolder.get(), IID_PPV_ARGS(desktopItem.put())));
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

  wil::com_ptr<IShellItem> shellItem;
  if (picker->GetResult(shellItem.put()) != S_OK) {
    return {};
  }

  wil::unique_cotaskmem_string buf;
  CheckHRESULT(shellItem->GetDisplayName(SIGDN_FILESYSPATH, buf.put()));
  return std::filesystem::path {std::wstring_view {buf.get()}};
}
std::vector<std::filesystem::path>
PlatformGUI_Windows::GetNewAPILayerJSONPaths() {
  const auto picker
    = wil::CoCreateInstance<IFileOpenDialog>(CLSID_FileOpenDialog);

  {
    FILEOPENDIALOGOPTIONS opts;
    picker->GetOptions(&opts);
    opts |= FOS_FORCEFILESYSTEM | FOS_FILEMUSTEXIST | FOS_PATHMUSTEXIST
      | FOS_ALLOWMULTISELECT;
    picker->SetOptions(opts);
  }

  {
    constexpr GUID persistenceGuid {
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

  wil::com_ptr<IShellItemArray> items;
  CheckHRESULT(picker->GetResults(items.put()));

  if (!items) {
    return {};
  }

  DWORD count {};
  CheckHRESULT(items->GetCount(&count));

  std::vector<std::filesystem::path> ret;

  for (DWORD i = 0; i < count; ++i) {
    wil::com_ptr<IShellItem> item;
    CheckHRESULT(items->GetItemAt(i, item.put()));
    wil::unique_cotaskmem_string buf;
    CheckHRESULT(item->GetDisplayName(SIGDN_FILESYSPATH, buf.put()));
    if (!buf) {
      continue;
    }

    ret.emplace_back(std::wstring_view {buf.get()});
  }
  return ret;
}

std::expected<LoaderData, std::string> PlatformGUI_Windows::GetLoaderData() {
  SECURITY_ATTRIBUTES saAttr {
    .nLength = sizeof(SECURITY_ATTRIBUTES),
    .bInheritHandle = TRUE,
  };

  wil::unique_handle stdoutRead;
  wil::unique_handle stdoutWrite;
  if (!CreatePipe(stdoutRead.put(), stdoutWrite.put(), &saAttr, 0)) {
    return std::unexpected("Failed to create pipe");
  }
  if (!SetHandleInformation(stdoutRead.get(), HANDLE_FLAG_INHERIT, 0)) {
    return std::unexpected("Failed to set pipe properties");
  }

  wchar_t modulePath[MAX_PATH];
  if (!GetModuleFileNameW(nullptr, modulePath, MAX_PATH)) {
    return std::unexpected("Failed to get executable path");
  }

  STARTUPINFOW si {
    .cb = sizeof(STARTUPINFOW),
    .dwFlags = STARTF_USESTDHANDLES,
    .hStdOutput = stdoutWrite.get(),
  };
  PROCESS_INFORMATION pi {};

  std::wstring cmdLine {modulePath};
  cmdLine += L" loader-query";

  if (!CreateProcessW(
        modulePath,
        cmdLine.data(),
        nullptr,
        nullptr,
        TRUE,
        0,
        nullptr,
        nullptr,
        &si,
        &pi)) {
    return std::unexpected("Failed to create process");
  }

  wil::unique_handle process {pi.hProcess};
  wil::unique_handle thread {pi.hThread};
  stdoutWrite.reset();

  std::string output;
  char buffer[4096];
  DWORD bytesRead;

  while (ReadFile(stdoutRead.get(), buffer, sizeof(buffer), &bytesRead, nullptr)
         && bytesRead > 0) {
    output.append(buffer, bytesRead);
  }

  WaitForSingleObject(process.get(), INFINITE);

  DWORD exitCode;
  if (!GetExitCodeProcess(process.get(), &exitCode) || exitCode != 0) {
    return std::unexpected(
      std::format("Subprocess failed with exit code {}", exitCode));
  }

  try {
    return static_cast<LoaderData>(nlohmann::json::parse(output));
  } catch (const nlohmann::json::exception& e) {
    const auto what = e.what();
    __debugbreak();
  }
}

void PlatformGUI_Windows::InitializeFonts(ImGuiIO* io) {
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
}

LRESULT PlatformGUI_Windows::WindowProc(
  HWND hWnd,
  const UINT uMsg,
  const WPARAM wParam,
  const LPARAM lParam) {
  auto self = reinterpret_cast<PlatformGUI_Windows*>(
    GetWindowLongPtrW(hWnd, GWLP_USERDATA));

  switch (uMsg) {
    case WM_CREATE:
    case WM_NCCREATE: {
      const auto create = reinterpret_cast<CREATESTRUCT*>(lParam);
      if (create->lpCreateParams) {
        self = static_cast<PlatformGUI_Windows*>(create->lpCreateParams);
        SetWindowLongPtrW(hWnd, GWLP_USERDATA, std::bit_cast<LONG_PTR>(self));
      }
      break;
    }
    case WM_GETMINMAXINFO: {
      const auto dpi = static_cast<LONG>(GetDpiForWindow(hWnd));
      const auto mmi = reinterpret_cast<MINMAXINFO*>(lParam);
      mmi->ptMinTrackSize = {
        (Config::MINIMUM_WINDOW_WIDTH * dpi) / USER_DEFAULT_SCREEN_DPI,
        (Config::MINIMUM_WINDOW_HEIGHT * dpi) / USER_DEFAULT_SCREEN_DPI,
      };
      break;
    }
    default:
      break;
  }

  if (self) {
    return self->InstanceWindowProc(hWnd, uMsg, wParam, lParam);
  }
  return DefWindowProcW(hWnd, uMsg, wParam, lParam);
}

LRESULT PlatformGUI_Windows::InstanceWindowProc(
  HWND hWnd,
  const UINT uMsg,
  const WPARAM wParam,
  const LPARAM lParam) {
  if (ImGui_ImplWin32_WndProcHandler(hWnd, uMsg, wParam, lParam)) {
    return true;
  }

  switch (uMsg) {
    case WM_DPICHANGED: {
      mDPIScaling
        = static_cast<float>(HIWORD(wParam)) / USER_DEFAULT_SCREEN_DPI;
      const auto rect = reinterpret_cast<RECT*>(lParam);
      mDPIChangeInfo = DPIChangeInfo {
        .mDPIScaling = mDPIScaling,
        .mRecommendedSize = ImVec2 {
          static_cast<float>(rect->right - rect->left),
          static_cast<float>(rect->bottom - rect->top),
        },
      };
      break;
    }
    case WM_SIZE: {
      DXGI_SWAP_CHAIN_DESC1 desc {};
      CheckHRESULT(mSwapChain->GetDesc1(&desc));

      mBackBuffer.reset();
      mRenderTargetView.reset();
      const auto w = LOWORD(lParam);
      const auto h = HIWORD(lParam);
      CheckHRESULT(
        mSwapChain->ResizeBuffers(0, w, h, DXGI_FORMAT_UNKNOWN, desc.Flags));
      CheckHRESULT(mSwapChain->GetBuffer(0, IID_PPV_ARGS(mBackBuffer.put())));
      CheckHRESULT(mD3DDevice->CreateRenderTargetView(
        mBackBuffer.get(), nullptr, mRenderTargetView.put()));
      mWindowSize = {static_cast<float>(w), static_cast<float>(h)};
      break;
    }
    case WM_CLOSE:
      mWindowClosed = true;
      break;
    default:
      break;
  }

  return DefWindowProcW(hWnd, uMsg, wParam, lParam);
}

PlatformGUI& PlatformGUI::Get() {
  static PlatformGUI_Windows sInstance {};
  return sInstance;
}

}// namespace FredEmmott::OpenXRLayers