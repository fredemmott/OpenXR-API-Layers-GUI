// SPDX-License-Identifier: ISC

#include <Windows.h>

#include "Config.hpp"
#include "Config_windows.hpp"
#include "Watcher.hpp"

namespace FredEmmott::OpenXRLayers {

static constexpr auto SUBKEY {
  L"SOFTWARE\\Khronos\\OpenXR\\1\\ApiLayers\\Implicit"};

class Watcher_Windows final : public Watcher {
 private:
  HKEY mKey {};
  HANDLE mEvent {};

 public:
  Watcher_Windows() {
    RegOpenKeyW(Config::APILAYER_HKEY_ROOT, SUBKEY, &mKey);
    mEvent = CreateEvent(nullptr, false, false, nullptr);
    this->Poll();
  }

  virtual ~Watcher_Windows() {
    RegCloseKey(mKey);
    CloseHandle(mEvent);
  }

  bool Poll() override {
    const auto ret = WaitForSingleObject(mEvent, 0);
    RegNotifyChangeKeyValue(
      mKey, false, REG_NOTIFY_CHANGE_LAST_SET, mEvent, true);
    return ret;
  };
};

Watcher& Watcher::Get() {
  static Watcher_Windows sInstance {};
  return sInstance;
}

}// namespace FredEmmott::OpenXRLayers