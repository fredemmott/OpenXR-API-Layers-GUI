// Copyright 2023 Fred Emmott <fred@fredemmott.com>
// SPDX-License-Identifier: ISC

#include "windows/WindowsPlatform.hpp"

#include <Unknwn.h>
#include <Windows.h>
#include <WinTrust.h>
#include <SoftPub.h>

#include <wil/com.h>
#include <wil/registry.h>
#include <wil/resource.h>

#include <fmt/format.h>
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
#include <userenv.h>

#include "APILayerStore.hpp"
#include "CheckForUpdates.hpp"
#include "Config.hpp"
#include "LoaderData.hpp"
#include "Platform.hpp"
#include "windows/GetKnownFolderPath.hpp"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(
  HWND hWnd,
  UINT msg,
  WPARAM wParam,
  LPARAM lParam);

namespace FredEmmott::OpenXRLayers {

namespace {

std::vector<AvailableRuntime> GetAvailableRuntimesByWowFlag(
  const REGSAM bitness) {
  const REGSAM desiredAccess = bitness | KEY_READ;
  wil::unique_hkey hkey;
  RegOpenKeyExW(
    HKEY_LOCAL_MACHINE,
    L"SOFTWARE\\Khronos\\OpenXR\\1\\AvailableRuntimes",
    0,
    desiredAccess,
    hkey.put());
  if (!hkey) {
    return {};
  }

  std::vector<AvailableRuntime> ret;
  using enum AvailableRuntime::Discoverability;
  for (auto&& it: wil::make_range(
         wil::reg::value_iterator {hkey.get()}, wil::reg::value_iterator {})) {
    if (it.type != REG_DWORD) {
      ret.emplace_back(it.name, Win32_NotDWORD);
      continue;
    }

    DWORD value {};
    DWORD valueSize {sizeof(value)};
    RegGetValueW(
      hkey.get(),
      nullptr,
      it.name.c_str(),
      RRF_RT_DWORD,
      nullptr,
      &value,
      &valueSize);
    ret.emplace_back(it.name, (value == 0) ? Discoverable : Hidden);
  }
  return ret;
}

static LSTATUS
GetActiveRuntimePath(const REGSAM desiredAccess, void* data, DWORD* dataSize) {
  wil::unique_hkey hkey;
  RegOpenKeyExW(
    HKEY_LOCAL_MACHINE,
    L"SOFTWARE\\Khronos\\OpenXR\\1",
    0,
    desiredAccess,
    hkey.put());
  return RegGetValueW(
    hkey.get(),
    nullptr,
    L"ActiveRuntime",
    RRF_RT_REG_SZ,
    nullptr,
    data,
    dataSize);
}

std::filesystem::path GetActiveRuntimePathByWowFlag(const REGSAM wowFlag) {
  const REGSAM desiredAccess = wowFlag | KEY_QUERY_VALUE;

  DWORD dataSize {0};
  if (
    GetActiveRuntimePath(desiredAccess, nullptr, &dataSize) != ERROR_SUCCESS) {
    return {};
  }
  if (dataSize == 0) {
    return {};
  }
  assert(dataSize % sizeof(wchar_t) == 0);

  std::wstring ret;
  ret.resize(dataSize / sizeof(WCHAR));
  GetActiveRuntimePath(desiredAccess, ret.data(), &dataSize);
  while (ret.back() == L'\0') {
    ret.pop_back();
  }

  return ret;
}

std::size_t WideCharToUTF8(
  wchar_t const* in,
  const std::size_t inCharCount,
  char* out,
  const std::size_t outByteCount) {
  return WideCharToMultiByte(
    CP_UTF8,
    0,
    in,
    static_cast<int>(inCharCount),
    out,
    static_cast<int>(outByteCount),
    nullptr,
    nullptr);
}

std::string WideCharToUTF8(const std::wstring_view in) {
  const auto byteCount = WideCharToUTF8(in.data(), in.size(), nullptr, 0);
  std::string ret;
  ret.resize_and_overwrite(byteCount + 1, [=](auto p, const auto size) {
    return WideCharToUTF8(in.data(), in.size(), p, size);
  });
  return ret;
}

}// namespace

void WindowsPlatform::InitializeDirect3D() {
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

void WindowsPlatform::GUIMain(const std::function<void()> drawFrame) {
  this->Initialize();
  const auto shutdown = wil::scope_exit([this] { this->Shutdown(); });
  this->MainLoop(drawFrame);
}

void WindowsPlatform::MainLoop(const std::function<void()>& drawFrame) {
  constexpr auto Interval
    = std::chrono::microseconds(1000000 / Config::MAX_FPS);

  mNewFrameEvent.create();
  const auto changeSubscriptions = APILayerStore::Get()
    | std::views::transform([e = mNewFrameEvent.get(), this](const auto store) {
                                     return store->OnChange([e, this] {
                                       {
                                         const std::unique_lock lock(mMutex);
                                         mLoaderDataIsStale = true;
                                         mLoaderData.clear();
                                       }
                                       mLoaderDataCondition.notify_all();
                                       SetEvent(e);
                                     });
                                   })
    | std::ranges::to<std::vector>();

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

    mNewFrameEvent.ResetEvent();

    {
      this->BeforeFrame();
      drawFrame();
      this->AfterFrame();
    }

    {
      const auto e = mNewFrameEvent.get();
      MsgWaitForMultipleObjects(1, &e, FALSE, INFINITE, QS_ALLINPUT);
    }
    const auto sleepFor = earliestNextFrame - std::chrono::steady_clock::now();
    if (sleepFor > std::chrono::steady_clock::duration::zero()) {
      std::this_thread::sleep_for(
        std::chrono::duration_cast<std::chrono::milliseconds>(sleepFor));
    }
  }
}

void WindowsPlatform::Initialize() {
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

void WindowsPlatform::Shutdown() {
  ImGui_ImplDX11_Shutdown();
  ImGui_ImplWin32_Shutdown();
  ImGui::DestroyContext();
}

void WindowsPlatform::BeforeFrame() {
  if ((++mFrameCounter % 60) == 0) {
    if (!mUpdater) {
      mUpdater = CheckForUpdates();
    }
    mUpdater->ActivateWindowIfVisible();
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

void WindowsPlatform::AfterFrame() {
  ImGui::Render();
  ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
  CheckHRESULT(mSwapChain->Present(0, 0));
}

void WindowsPlatform::ShowFolderContainingFile(
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

std::map<std::string, std::string> WindowsPlatform::GetEnvironmentVariables() {
  std::map<std::string, std::string> ret;

  wil::unique_environstrings_ptr env {GetEnvironmentStringsW()};
  std::string buf;

  for (auto it = env.get(); (it && *it); it += (wcslen(it) + 1)) {
    const auto nameEnd = std::wstring_view {it}.find(L'=');
    if (nameEnd == 0 || nameEnd == std::wstring_view::npos) {
      continue;
    }

    ret.emplace(
      WideCharToUTF8({it, nameEnd}), WideCharToUTF8({it + nameEnd + 1}));
  }
  return ret;
}

std::vector<AvailableRuntime> WindowsPlatform::GetAvailableRuntimes(
  const Architecture arch) {
  switch (arch) {
    case Architecture::x64:
      return GetAvailableRuntimesByWowFlag(KEY_WOW64_64KEY);
    case Architecture::x86:
      return GetAvailableRuntimesByWowFlag(KEY_WOW64_32KEY);
    default:
      throw std::logic_error("Unsupported architecture");
  }
}
std::filesystem::file_time_type WindowsPlatform::GetFileChangeTime(
  const std::filesystem::path& path) {
  const wil::unique_hfile file {CreateFileW(
    path.wstring().c_str(),
    GENERIC_READ,
    FILE_SHARE_READ,
    nullptr,
    OPEN_EXISTING,
    FILE_FLAG_BACKUP_SEMANTICS,
    nullptr)};
  FILE_BASIC_INFO info {};
  GetFileInformationByHandleEx(file.get(), FileBasicInfo, &info, sizeof(info));
  return std::filesystem::file_time_type {std::chrono::file_clock::duration {
    std::bit_cast<int64_t>(info.ChangeTime)}};
}

std::expected<APILayerSignature, APILayerSignature::Error>
WindowsPlatform::GetAPILayerSignature(const std::filesystem::path& dllPath) {
  using enum APILayerSignature::Error;
  if (!std::filesystem::exists(dllPath)) {
    return std::unexpected {FilesystemError};
  }

  WINTRUST_FILE_INFO fileInfo {
    .cbStruct = sizeof(WINTRUST_FILE_INFO),
    .pcwszFilePath = dllPath.c_str(),
  };
  WINTRUST_DATA wintrustData {
    .cbStruct = sizeof(WINTRUST_DATA),
    .dwUIChoice = WTD_UI_NONE,
    .fdwRevocationChecks = WTD_REVOCATION_CHECK_NONE,
    .dwUnionChoice = WTD_CHOICE_FILE,
    .pFile = &fileInfo,
    .dwStateAction = WTD_STATEACTION_VERIFY,
  };

  GUID policyGuid = WINTRUST_ACTION_GENERIC_VERIFY_V2;
  const auto result = WinVerifyTrust(
    static_cast<HWND>(INVALID_HANDLE_VALUE), &policyGuid, &wintrustData);
  const auto close = wil::scope_exit([&] {
    wintrustData.dwStateAction = WTD_STATEACTION_CLOSE;
    WinVerifyTrust(
      static_cast<HWND>(INVALID_HANDLE_VALUE), &policyGuid, &wintrustData);
  });

  switch (result) {
    case 0:
      break;
    case TRUST_E_SUBJECT_NOT_TRUSTED:
      return std::unexpected {UntrustedSignature};
    case CERT_E_EXPIRED:
      return std::unexpected {Expired};
    case TRUST_E_NOSIGNATURE:
    default:
      return std::unexpected {Unsigned};
  }

  const auto providerData
    = WTHelperProvDataFromStateData(wintrustData.hWVTStateData);
  const auto signer = WTHelperGetProvSignerFromChain(providerData, 0, FALSE, 0);
  const auto cert = WTHelperGetProvCertFromChain(signer, 0);

  const auto signerUtf16Count = CertGetNameStringW(
    cert->pCert, CERT_NAME_SIMPLE_DISPLAY_TYPE, 0, nullptr, nullptr, 0);
  std::wstring wideSignerName;
  wideSignerName.resize_and_overwrite(
    signerUtf16Count + 1, [=](auto p, const auto size) {
      return CertGetNameStringW(
               cert->pCert, CERT_NAME_SIMPLE_DISPLAY_TYPE, 0, nullptr, p, size)
        - 1;
    });

  const auto fileClockTimePoint
    = std::chrono::file_clock::time_point {std::chrono::file_clock::duration {
      static_cast<int64_t>(signer->sftVerifyAsOf.dwHighDateTime) << 32
      | signer->sftVerifyAsOf.dwLowDateTime}};

  return APILayerSignature {
    WideCharToUTF8(wideSignerName),
    std::chrono::clock_cast<std::chrono::system_clock>(fileClockTimePoint),
  };
}

std::vector<std::string> WindowsPlatform::GetEnabledExplicitAPILayers() {
  const auto charCount
    = GetEnvironmentVariableW(L"XR_ENABLE_API_LAYERS", nullptr, 0);
  if (charCount < 2) {
    // Error (e.g. no var) or just a trailing null
    return {};
  }
  std::wstring envVar(charCount, L'\0');
  GetEnvironmentVariableW(L"XR_ENABLE_API_LAYERS", envVar.data(), charCount);
  assert(envVar.back() == L'\0');
  envVar.pop_back();

  std::vector<std::string> ret;
  for (auto&& layer: std::views::split(WideCharToUTF8(envVar), ';')) {
    ret.emplace_back(std::string_view {layer});
  }
  return ret;
}
std::optional<std::vector<std::filesystem::path>>
WindowsPlatform::GetOverridePaths() const {
  const auto charCount
    = GetEnvironmentVariableW(L"XR_API_LAYER_PATH", nullptr, 0);
  if (charCount == 0) {
    return std::nullopt;
  }
  if (charCount == 1) {
    // Just a trailing null
    return {};
  }
  std::wstring envVar(charCount, L'\0');
  GetEnvironmentVariableW(L"XR_API_LAYER_PATH", envVar.data(), charCount);
  assert(envVar.back() == L'\0');
  envVar.pop_back();

  std::vector<std::filesystem::path> ret;
  for (auto&& path: std::views::split(WideCharToUTF8(envVar), ';')) {
    ret.emplace_back(std::filesystem::path {std::string_view {path}});
  }
  return ret;
}

Architectures WindowsPlatform::GetArchitectures() const {
  constexpr auto Current = GetBuildArchitecture();
  static_assert(
    Current == Architecture::x64 || Current == Architecture::x86,
    "Unsupported architecture");
  return Architecture::x64 | Architecture::x86;
}
Architectures WindowsPlatform::GetSharedLibraryArchitectures(
  const std::filesystem::path& path) const {
  const wil::unique_hfile file {CreateFileW(
    path.wstring().c_str(),
    GENERIC_READ,
    FILE_SHARE_READ,
    nullptr,
    OPEN_EXISTING,
    NULL,
    nullptr)};
  if (!file) {
    return {};
  }

  const wil::unique_handle mapping {
    CreateFileMappingW(file.get(), nullptr, PAGE_READONLY, 0, 0, nullptr)};
  if (!mapping) {
    return {};
  }
  const wil::unique_mapview_ptr<const IMAGE_DOS_HEADER> dosHeader {
    static_cast<const IMAGE_DOS_HEADER*>(
      MapViewOfFile(mapping.get(), FILE_MAP_READ, 0, 0, 0))};
  if (!dosHeader) {
    return {};
  }
  if (dosHeader->e_magic != IMAGE_DOS_SIGNATURE) {
    return {};
  }
  const auto base = reinterpret_cast<uintptr_t>(dosHeader.get());
  const auto ntHeaders
    = reinterpret_cast<const IMAGE_NT_HEADERS*>(base + dosHeader->e_lfanew);
  if (ntHeaders->Signature != IMAGE_NT_SIGNATURE) {
    return {};
  }

  switch (ntHeaders->FileHeader.Machine) {
    case IMAGE_FILE_MACHINE_I386:
      return Architecture::x86;
    case IMAGE_FILE_MACHINE_AMD64:
      return Architecture::x64;
    default:
      return {};
  }
}

HWND WindowsPlatform::CreateAppWindow() {
  static const auto WindowTitle
    = std::format(L"OpenXR API Layers v{}", Config::BUILD_VERSION_W);
  static const WNDCLASSEXW WindowClass {
    .cbSize = sizeof(WNDCLASSEXW),
    .lpfnWndProc = &WindowsPlatform::WindowProc,
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

std::optional<std::filesystem::path> WindowsPlatform::GetExportFilePath() {
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

std::vector<std::filesystem::path> WindowsPlatform::GetNewAPILayerJSONPaths() {
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

template <class T>
static std::unexpected<T> UnexpectedHRESULT(HRESULT value) {
  return std::unexpected {
    T {{value, std::system_category()}},
  };
}

template <class T>
static std::unexpected<T> UnexpectedGetLastError() {
  return UnexpectedHRESULT<T>(HRESULT_FROM_WIN32(GetLastError()));
}

std::expected<LoaderData, LoaderData::Error> WindowsPlatform::GetLoaderData(
  const Architecture arch) {
  assert(GetArchitectures().contains(arch));
  EnsureLoaderDataThread();
  const std::unique_lock lock(mMutex);
  if (mLoaderDataIsStale || !mLoaderData.contains(arch)) {
    return std::unexpected {LoaderData::PendingError {}};
  }
  return mLoaderData.at(arch);
}
std::expected<LoaderData, LoaderData::Error> WindowsPlatform::WaitForLoaderData(
  const Architecture arch,
  const std::chrono::steady_clock::time_point timeout) {
  assert(GetArchitectures().contains(arch));
  EnsureLoaderDataThread();

  std::unique_lock lock(mMutex);
  if (mLoaderData.contains(arch)) {
    const auto& data = mLoaderData.at(arch);
    if (data) {
      return data;
    }
    if (!holds_alternative<LoaderData::PendingError>(data.error())) {
      return data;
    }
  }

  const auto completed = mLoaderDataCondition.wait_until(
    lock, timeout, [&] { return mLoaderData.contains(arch); });
  if (!completed) {
    return std::unexpected {LoaderData::PendingError {}};
  }

  return mLoaderData.at(arch);
}

void WindowsPlatform::EnsureLoaderDataThread() {
  if (mLoaderDataThread.joinable()) {
    return;
  }
  mLoaderDataThread = std::jthread {
    std::bind_front(&WindowsPlatform::LoaderDataThreadMain, this)};
}
void WindowsPlatform::LoaderDataThreadMain(const std::stop_token token) {
  SetThreadDescription(GetCurrentThread(), L"LoaderData Thread");

  // Get the shell token to de-elevate
  {
    wil::unique_handle processToken;
    // Gain SE_DEBUG_NAME so we can attach to explorer; we need this because
    // the loader will ignore XR_API_LAYER_PATHS if elevated
    OpenProcessToken(
      GetCurrentProcess(),
      TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY,
      processToken.put());
    LUID luid {};
    LookupPrivilegeValueW(NULL, SE_DEBUG_NAME, &luid);
    TOKEN_PRIVILEGES tp {};
    tp.PrivilegeCount = 1;
    tp.Privileges[0].Luid = luid;
    tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
    AdjustTokenPrivileges(
      processToken.get(), FALSE, &tp, sizeof(tp), nullptr, nullptr);
  }
  wil::unique_handle subprocessToken;
  {
    DWORD processId;
    GetWindowThreadProcessId(GetShellWindow(), &processId);
    wil::unique_handle shell {
      OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, processId)};
    wil::unique_handle token;
    OpenProcessToken(shell.get(), TOKEN_DUPLICATE, token.put());
    DuplicateTokenEx(
      token.get(),
      TOKEN_ALL_ACCESS,
      nullptr,
      SecurityImpersonation,
      TokenPrimary,
      subprocessToken.put());
  }

  mLoaderDataJob.reset(CreateJobObjectW(nullptr, nullptr));
  {
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION limit {};
    limit.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    CheckHRESULT(SetInformationJobObject(
      mLoaderDataJob.get(),
      JobObjectExtendedLimitInformation,
      &limit,
      sizeof(limit)));
  };

  while (true) {
    {
      std::unique_lock lock(mMutex);
      const auto stopped = !mLoaderDataCondition.wait(
        lock, token, [&] { return mLoaderDataIsStale; });
      if (stopped)
        return;

      mLoaderDataIsStale = false;
    }

    std::unordered_map<Architecture, LoaderDataProcess> pending;
    decltype(mLoaderData) newLoaderData;
    for (const auto arch: GetArchitectures().enumerate()) {
      auto subprocess = [&] {
        return SpawnLoaderData(
          mLoaderDataJob.get(), subprocessToken.get(), arch);
      }();
      if (!subprocess) {
        newLoaderData.emplace(
          arch, std::unexpected {std::move(subprocess).error()});
        continue;
      }
      pending.emplace(arch, std::move(*subprocess));
    }

    for (auto&& [arch, process]: pending) {
      newLoaderData.emplace(arch, std::move(process).Wait());
#ifndef NDEBUG
      if (
        newLoaderData.at(arch) && newLoaderData.at(arch)->mArchitecture != arch)
        [[unlikely]] {
        throw std::runtime_error(
          fmt::format(
            "Architecture mismatch on loader data - got {}, expected {}",
            magic_enum::enum_name(newLoaderData.at(arch)->mArchitecture),
            magic_enum::enum_name(arch)));
      }
#endif
    }

    {
      std::unique_lock lock(mMutex);
      mLoaderData = std::move(newLoaderData);
    }

    mLoaderDataCondition.notify_all();
    mOnLoaderDataSignal();
    mNewFrameEvent.SetEvent();
  }
}

std::expected<WindowsPlatform::LoaderDataProcess, LoaderData::Error>
WindowsPlatform::SpawnLoaderData(
  HANDLE const hJob,
  HANDLE const token,
  Architecture const arch) {
  SECURITY_ATTRIBUTES saAttr {
    .nLength = sizeof(SECURITY_ATTRIBUTES),
    .bInheritHandle = TRUE,
  };

  wil::unique_handle stdoutRead;
  wil::unique_handle stdoutWrite;
  if (!CreatePipe(stdoutRead.put(), stdoutWrite.put(), &saAttr, 0)) {
    return UnexpectedGetLastError<LoaderData::PipeCreationError>();
  }
  if (!SetHandleInformation(stdoutRead.get(), HANDLE_FLAG_INHERIT, 0)) {
    return UnexpectedGetLastError<LoaderData::PipeAttributeError>();
  }

  constexpr auto MaxPathExtended = 32768;
  wchar_t modulePath[MaxPathExtended];
  if (!GetModuleFileNameW(nullptr, modulePath, MaxPathExtended)) {
    return UnexpectedGetLastError<
      LoaderData::CanNotFindCurrentExecutableError>();
  }

  STARTUPINFOW si {
    .cb = sizeof(STARTUPINFOW),
    .dwFlags = STARTF_USESTDHANDLES,
    .hStdOutput = stdoutWrite.get(),
  };
  PROCESS_INFORMATION pi {};

  const auto commandLine
    = (std::filesystem::path {modulePath}.parent_path()
       / std::format("openxr-loader-data-{}.dll", magic_enum::enum_name(arch)))
        .wstring();
  void* environment {};
  CreateEnvironmentBlock(&environment, token, /* INHERIT = */ TRUE);
  const auto freeEnvironment
    = wil::scope_exit([environment] { DestroyEnvironmentBlock(environment); });
  if (!CreateProcessWithTokenW(
        token,
        LOGON_WITH_PROFILE,
        commandLine.c_str(),
        nullptr,
        CREATE_NO_WINDOW | CREATE_UNICODE_ENVIRONMENT,
        environment,
        nullptr,
        &si,
        &pi)) {
    return UnexpectedGetLastError<LoaderData::CanNotSpawnError>();
  }
  AssignProcessToJobObject(hJob, pi.hProcess);
  stdoutWrite.reset();

  return LoaderDataProcess {
    .mProcess = wil::unique_handle {pi.hProcess},
    .mThread = wil::unique_handle {pi.hThread},
    .mStdoutReadPipe = std::move(stdoutRead),
  };
}

std::expected<LoaderData, LoaderData::Error>
WindowsPlatform::LoaderDataProcess::Wait() && {
  if (!(mProcess && mStdoutReadPipe)) {
    throw std::logic_error(
      "LoaderDataProcess::Wait() called with invalid handles - maybe moved?");
  }
  // We take a move-reference to self, so let's clean up when we're done
  const auto release = wil::scope_exit([this] { *this = {}; });
  std::string output;
  char buffer[4096];
  DWORD bytesRead;

  while (
    ReadFile(mStdoutReadPipe.get(), buffer, sizeof(buffer), &bytesRead, nullptr)
    && bytesRead > 0) {
    output.append(buffer, bytesRead);
  }

  WaitForSingleObject(mProcess.get(), INFINITE);

  DWORD exitCode;
  if (!GetExitCodeProcess(mProcess.get(), &exitCode) || exitCode != 0) {
    return std::unexpected {LoaderData::BadExitCodeError {exitCode}};
  }

  try {
    return static_cast<LoaderData>(nlohmann::json::parse(output));
  } catch (const nlohmann::json::exception& e) {
    return std::unexpected {LoaderData::InvalidJSONError {e.what()}};
  }
}

void WindowsPlatform::InitializeFonts(ImGuiIO* io) {
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

LRESULT WindowsPlatform::WindowProc(
  HWND hWnd,
  const UINT uMsg,
  const WPARAM wParam,
  const LPARAM lParam) {
  auto self = reinterpret_cast<WindowsPlatform*>(
    GetWindowLongPtrW(hWnd, GWLP_USERDATA));

  switch (uMsg) {
    case WM_CREATE:
    case WM_NCCREATE: {
      const auto create = reinterpret_cast<CREATESTRUCT*>(lParam);
      if (create->lpCreateParams) {
        self = static_cast<WindowsPlatform*>(create->lpCreateParams);
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

LRESULT WindowsPlatform::InstanceWindowProc(
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

Platform& Platform::Get() {
  static WindowsPlatform sInstance {};
  return sInstance;
}

std::filesystem::path WindowsPlatform::GetActiveRuntimePath(
  const Architecture arch) {
  switch (arch) {
    case Architecture::x86:
      return GetActiveRuntimePathByWowFlag(KEY_WOW64_32KEY);
    case Architecture::x64:
      return GetActiveRuntimePathByWowFlag(KEY_WOW64_64KEY);
    default:
      throw std::logic_error("Unsupported architecture");
  }
}

}// namespace FredEmmott::OpenXRLayers