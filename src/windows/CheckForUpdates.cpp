// Copyright 2023 Fred Emmott <fred@fredemmott.com>
// SPDX-License-Identifier: ISC

#include "CheckForUpdates.hpp"

#include <Windows.h>
#include <Psapi.h>

#include <winrt/base.h>

#include <filesystem>
#include <format>
#include <memory>

#include "Config.hpp"
#include "windows/check.hpp"

namespace FredEmmott::OpenXRLayers {
namespace {
class HandleInOtherProcess {
 public:
  HandleInOtherProcess() = delete;
  HandleInOtherProcess(const HandleInOtherProcess&) = delete;
  HandleInOtherProcess(HandleInOtherProcess&&) = delete;
  HandleInOtherProcess& operator=(const HandleInOtherProcess&) = delete;
  HandleInOtherProcess& operator=(HandleInOtherProcess&&) = delete;

  HandleInOtherProcess(
    HANDLE sourceHandle,
    HANDLE targetProcess,
    DWORD desiredAccess)
    : mTargetProcess(targetProcess) {
    CheckBOOL(DuplicateHandle(
      GetCurrentProcess(),
      sourceHandle,
      targetProcess,
      &mHandleInTargetProcess,
      desiredAccess,
      /* bInheritHandle = */ true,
      0));
  }

  ~HandleInOtherProcess() {
    HANDLE handleInThisProcess {nullptr};
    CheckBOOL(DuplicateHandle(
      mTargetProcess,
      mHandleInTargetProcess,
      GetCurrentProcess(),
      &handleInThisProcess,
      0,
      FALSE,
      DUPLICATE_CLOSE_SOURCE));
    CloseHandle(handleInThisProcess);
  }

  [[nodiscard]]
  HANDLE get() const noexcept {
    return mHandleInTargetProcess;
  }

 private:
  HANDLE mTargetProcess {nullptr};
  HANDLE mHandleInTargetProcess {nullptr};
};
}// namespace

AutoUpdateProcess CheckForUpdates() {
  wchar_t exePathStr[32767];
  const auto exePathStrLength = GetModuleFileNameExW(
    GetCurrentProcess(), nullptr, exePathStr, std::size(exePathStr));
  const std::filesystem::path thisExe {
    std::wstring_view {exePathStr, exePathStrLength}};
  const auto directory = thisExe.parent_path();

  const auto updater
    = directory / L"fredemmott_OpenXR-API-Layers-GUI_Updater.exe";

  if (!std::filesystem::exists(updater)) {
    OutputDebugStringW(
      std::format(
        L"Skipping auto-update because `{}` does not exist", updater.wstring())
        .c_str());
    return {};
  }

  // - We run elevated as the entire point of this program is to write to HKLM
  // - We don't want to elevate the installer unnecessarily
  //
  // So, we get the shell window/process so we can use
  // PROC_THREAD_ATTRIBUTE_PARENT_PROCESS to de-elevate the updater

  const auto shellWindow = GetShellWindow();
  DWORD shellPid {};
  GetWindowThreadProcessId(shellWindow, &shellPid);
  wil::unique_process_handle shellProcess {
    OpenProcess(PROCESS_CREATE_PROCESS | PROCESS_DUP_HANDLE, FALSE, shellPid)};

  SIZE_T threadAttributesSize {};
  InitializeProcThreadAttributeList(nullptr, 1, 0, &threadAttributesSize);
  auto threadAttributesBuffer = std::make_unique<char[]>(threadAttributesSize);
  auto threadAttributes = reinterpret_cast<PPROC_THREAD_ATTRIBUTE_LIST>(
    threadAttributesBuffer.get());
  CheckBOOL(InitializeProcThreadAttributeList(
    threadAttributes, 1, 0, &threadAttributesSize));

  // Need a pointer to the value, not just the value
  HANDLE shellProcessRaw {shellProcess.get()};
  CheckBOOL(UpdateProcThreadAttribute(
    threadAttributes,
    0,
    PROC_THREAD_ATTRIBUTE_PARENT_PROCESS,
    &shellProcessRaw,
    sizeof(HANDLE),
    nullptr,
    nullptr));

  // As we're specifying PROC_THREAD_ATTRIBUTE_PARENT_PROCESS as the shell, we
  // need to duplicate the handle to the shell too, rather than the current
  // process.
  HandleInOtherProcess thisProcessAsShell {
    GetCurrentProcess(),
    shellProcessRaw,
    PROCESS_TERMINATE | PROCESS_QUERY_LIMITED_INFORMATION,
  };

  // Process group, including temporary children
  wil::unique_handle job {CreateJobObjectW(nullptr, nullptr)};
  // Jobs are only signalled on timeout, not on completion, so...
  wil::unique_handle jobCompletion {
    CreateIoCompletionPort(INVALID_HANDLE_VALUE, nullptr, 0, 1)};
  JOBOBJECT_ASSOCIATE_COMPLETION_PORT assoc {
    .CompletionKey = job.get(),
    .CompletionPort = jobCompletion.get(),
  };
  CheckBOOL(SetInformationJobObject(
    job.get(),
    JobObjectAssociateCompletionPortInformation,
    &assoc,
    sizeof(assoc)));

  const auto launch = [&]<class... Args> /* C++23 :'( [[nodiscard]] */ (
                        std::wformat_string<Args...> commandLineFmt,
                        Args&&... commandLineFmtArgs) -> HRESULT {
    auto commandLine
      = std::format(commandLineFmt, std::forward<Args>(commandLineFmtArgs)...);

    STARTUPINFOEXW startupInfo {
      .StartupInfo = {sizeof(STARTUPINFOEXW)},
      .lpAttributeList = threadAttributes,
    };
    PROCESS_INFORMATION processInfo {};

    if (!CreateProcessW(
          updater.wstring().c_str(),
          commandLine.data(),
          nullptr,
          nullptr,
          /* inherit handles = */ true,
          EXTENDED_STARTUPINFO_PRESENT | CREATE_SUSPENDED,
          nullptr,
          directory.wstring().c_str(),
          &startupInfo.StartupInfo,
          &processInfo)) {
      const auto hr = HRESULT_FROM_WIN32(GetLastError());
      const auto error = std::format(
        L"Updater CreateProcessW() failed with {:#018x}",
        static_cast<uint32_t>(hr));
      OutputDebugStringW(error.c_str());
      return hr;
    }
    CheckBOOL(AssignProcessToJobObject(job.get(), processInfo.hProcess));
    ResumeThread(processInfo.hThread);
    CloseHandle(processInfo.hProcess);
    CloseHandle(processInfo.hThread);
    return S_OK;
  };

  if (FAILED(launch(
        L"{} --channel=live --terminate-process-before-update={} "
        L"--local-version={} --silent",
        updater.wstring(),
        reinterpret_cast<uintptr_t>(thisProcessAsShell.get()),
        FredEmmott::OpenXRLayers::Config::BUILD_VERSION_W))) {
    return {};
  }

  return {std::move(job), std::move(jobCompletion)};
}

AutoUpdateProcess::AutoUpdateProcess(
  wil::unique_handle job,
  wil::unique_handle jobCompletion)
  : mJob(std::move(job)),
    mJobCompletion(std::move(jobCompletion)) {}

void AutoUpdateProcess::ActivateWindowIfVisible() {
  if (mHaveActivatedWindow || !mJob) {
    return;
  }

  if (!IsRunning()) {
    *this = {};
    return;
  }

  EnumWindows(
    [](HWND hwnd, LPARAM param) -> BOOL {
      auto& self = *std::bit_cast<decltype(this)>(param);

      DWORD processId {};
      if (!GetWindowThreadProcessId(hwnd, &processId)) {
        return TRUE;
      }

      const wil::unique_process_handle process {
        OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, processId)};
      if (!process) {
        return TRUE;
      }

      BOOL isInJob {FALSE};
      if (!(IsProcessInJob(process.get(), self.mJob.get(), &isInJob)
            && isInJob)) {
        return TRUE;
      }

      SetForegroundWindow(hwnd);
      self.mHaveActivatedWindow = true;

      return FALSE;
    },
    std::bit_cast<LPARAM>(this));
}

bool AutoUpdateProcess::IsRunning() const {
  if (!mJob) {
    return false;
  };

  DWORD completionCode {};
  ULONG_PTR completionKey {};
  LPOVERLAPPED overlapped {};
  if (!GetQueuedCompletionStatus(
        mJobCompletion.get(),
        &completionCode,
        &completionKey,
        &overlapped,
        NULL)) {
    return false;
  }

  return !(
    completionKey == reinterpret_cast<ULONG_PTR>(mJob.get())
    && completionCode == JOB_OBJECT_MSG_ACTIVE_PROCESS_ZERO);
}

}// namespace FredEmmott::OpenXRLayers