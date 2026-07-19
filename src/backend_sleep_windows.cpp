// Windows sleep backend: power request objects.
//
// PowerCreateRequest + PowerSetRequest(PowerRequestSystemRequired) is the modern
// replacement for SetThreadExecutionState. We prefer it here for two reasons:
//   * it carries a human-readable reason string, visible in `powercfg /requests`
//     (the Windows analogue of `systemd-inhibit --list` / `pmset -g assertions`);
//   * the request is owned by a process-scoped HANDLE rather than the calling
//     thread, so it is safe for a movable RAII object whose inhibit() and
//     uninhibit() may run on different threads.
//
// PowerRequestSystemRequired prevents automatic idle sleep -- the same level as
// the old ES_SYSTEM_REQUIRED -- while leaving the display / screensaver alone.

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0601  // Windows 7: PowerCreateRequest and friends
#endif
#ifndef NOMINMAX
#define NOMINMAX  // keep <windows.h> from defining min()/max() macros that
#endif            // would clobber the STL headers pulled in by backend.hpp

#include <windows.h>

#include <string>

#include "backend.hpp"

namespace woke::detail {

namespace {

// Convert UTF-8 to a native UTF-16 wide string. In a libtransmission build this
// one line can delegate to tr_win32_utf8_to_native(); it is inlined here so the
// library stays free of external dependencies.
std::wstring utf8_to_wide(const std::string& text) {
  if (text.empty()) return std::wstring();
  const int needed = ::MultiByteToWideChar(
      CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0);
  if (needed <= 0) return std::wstring();
  std::wstring wide(static_cast<size_t>(needed), L'\0');
  ::MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()),
                        wide.data(), needed);
  return wide;
}

class WindowsSleepBackend final : public Backend {
public:
  ~WindowsSleepBackend() override { uninhibit(); }

  bool inhibit(const std::string& who, const std::string& reason) override {
    if (active_) return true;

    // The power request exposes a single reason string, so fold the application
    // identity and the reason into one label.
    reason_ = utf8_to_wide(combine_label(who, reason));

    REASON_CONTEXT context = {};
    context.Version = POWER_REQUEST_CONTEXT_VERSION;
    context.Flags = POWER_REQUEST_CONTEXT_SIMPLE_STRING;
    context.Reason.SimpleReasonString = const_cast<PWSTR>(reason_.c_str());

    request_ = ::PowerCreateRequest(&context);
    if (request_ == nullptr || request_ == INVALID_HANDLE_VALUE) {
      request_ = nullptr;
      return false;
    }

    if (!::PowerSetRequest(request_, PowerRequestSystemRequired)) {
      ::CloseHandle(request_);
      request_ = nullptr;
      return false;
    }

    active_ = true;
    return true;
  }

  void uninhibit() noexcept override {
    if (request_ != nullptr) {
      if (active_) ::PowerClearRequest(request_, PowerRequestSystemRequired);
      ::CloseHandle(request_);
      request_ = nullptr;
    }
    active_ = false;
  }

  bool active() const noexcept override { return active_; }

private:
  HANDLE request_ = nullptr;
  std::wstring reason_;  // kept alive for the lifetime of the request
  bool active_ = false;
};

}  // namespace

std::unique_ptr<Backend> make_sleep_backend() {
  return std::make_unique<WindowsSleepBackend>();
}

const char* sleep_backend_name() { return "windows"; }

}  // namespace woke::detail
