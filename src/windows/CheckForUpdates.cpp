// Copyright 2023 Fred Emmott <fred@fredemmott.com>
// SPDX-License-Identifier: ISC

#include <Windows.h>
#include <Psapi.h>

#include <winrt/base.h>

#include <filesystem>
#include <format>
#include <memory>

#include "Config.hpp"

#include "CheckForUpdates.hpp"

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
    winrt::check_bool(DuplicateHandle(
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
    winrt::check_bool(DuplicateHandle(
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
  HANDLE get() const noexcept { return mHandleInTargetProcess; }

 private:
  HANDLE mTargetProcess {nullptr};
  HANDLE mHandleInTargetProcess {nullptr};
};
}// namespace

namespace FredEmmott::OpenXRLayers {

void CheckForUpdates() {
  wchar_t exePathStr[32767];
  const auto exePathStrLength = GetModuleFileNameExW(
    GetCurrentProcess(), nullptr, exePathStr, std::size(exePathStr));
  const std::filesystem::path thisExe {
    std::wstring_view {exePathStr, exePathStrLength}};
  const auto directory = thisExe.parent_path();

  const auto updater = directory / L"fredemmott_OpenXR-API-Layers-GUI_Updater.exe";

  if (!std::filesystem::exists(updater)) {
    OutputDebugStringW(
      std::format(
        L"Skipping auto-update because `{}` does not exist", updater.wstring())
        .c_str());
    return;
  }

  // - We run elevated as the entire point of this program is to write to HKLM
  // - We don't want to elevate the installer unnecessarily
  //
  // So, we get the shell window/process so we can use
  // PROC_THREAD_ATTRIBUTE_PARENT_PROCESS to de-elevate the updater

  const auto shellWindow = GetShellWindow();
  DWORD shellPid {};
  GetWindowThreadProcessId(shellWindow, &shellPid);
  winrt::handle shellProcess {
    OpenProcess(PROCESS_CREATE_PROCESS | PROCESS_DUP_HANDLE, FALSE, shellPid)};

  SIZE_T threadAttributesSize {};
  InitializeProcThreadAttributeList(nullptr, 1, 0, &threadAttributesSize);
  auto threadAttributesBuffer = std::make_unique<char[]>(threadAttributesSize);
  auto threadAttributes = reinterpret_cast<PPROC_THREAD_ATTRIBUTE_LIST>(
    threadAttributesBuffer.get());
  winrt::check_bool(InitializeProcThreadAttributeList(
    threadAttributes, 1, 0, &threadAttributesSize));

  // Need a pointer to the value, not just the value
  HANDLE shellProcessRaw {shellProcess.get()};
  winrt::check_bool(UpdateProcThreadAttribute(
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
    GetCurrentProcess(),shellProcessRaw,
    PROCESS_TERMINATE | PROCESS_QUERY_LIMITED_INFORMATION,
  };

  // Process group, including temporary children
  winrt::handle job { CreateJobObjectW(nullptr, nullptr) };
  // Jobs are only signalled on timeout, not on completion, so...
  winrt::handle jobCompletion {
    CreateIoCompletionPort(INVALID_HANDLE_VALUE, nullptr, 0, 1)
  };
  JOBOBJECT_ASSOCIATE_COMPLETION_PORT assoc {
    .CompletionKey = job.get(),
    .CompletionPort = jobCompletion.get(),
  };
  winrt::check_bool(SetInformationJobObject(job.get(),
    JobObjectAssociateCompletionPortInformation, &assoc, sizeof(assoc)));

  auto launch = [&]<class... Args>(
                  std::wformat_string<Args...> commandLineFmt,
                  Args&&... commandLineFmtArgs) {
    auto commandLine
      = std::format(commandLineFmt, std::forward<Args>(commandLineFmtArgs)...);

    STARTUPINFOEXW startupInfo {
      .StartupInfo = {sizeof(STARTUPINFOEXW)},
      .lpAttributeList = threadAttributes,
    };
    PROCESS_INFORMATION processInfo {};

    winrt::check_bool(CreateProcessW(
      updater.wstring().c_str(),
      commandLine.data(),
      nullptr,
      nullptr,
      /* inherit handles = */ true,
      EXTENDED_STARTUPINFO_PRESENT | CREATE_SUSPENDED,
      nullptr,
      directory.wstring().c_str(),
      &startupInfo.StartupInfo,
      &processInfo));
    winrt::check_bool(AssignProcessToJobObject(job.get(), processInfo.hProcess));
    ResumeThread(processInfo.hThread);
    WaitForSingleObject(processInfo.hProcess, INFINITE);
    CloseHandle(processInfo.hProcess);
    CloseHandle(processInfo.hThread);
  };

  launch(
    L"{} --channel=live --terminate-process-before-update={} --local-version={} --silent",
    updater.wstring(),
    reinterpret_cast<uintptr_t>(thisProcessAsShell.get()),
    FredEmmott::OpenXRLayers::Config::BUILD_VERSION_W);

  DWORD completionCode {};
  ULONG_PTR completionKey {};
  LPOVERLAPPED overlapped {};
  while (GetQueuedCompletionStatus(jobCompletion.get(), &completionCode, &completionKey, &overlapped, INFINITE) && !(
    completionKey == reinterpret_cast<ULONG_PTR>(job.get()) && completionCode == JOB_OBJECT_MSG_ACTIVE_PROCESS_ZERO)) {}
}

}