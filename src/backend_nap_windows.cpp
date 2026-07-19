// Windows nap backend: opt this process out of power throttling (EcoQoS).
//
// SetProcessInformation(ProcessPowerThrottling) with
// PROCESS_POWER_THROTTLING_EXECUTION_SPEED in ControlMask and a cleared
// StateMask tells the scheduler not to throttle this process's CPU when it
// looks idle / background -- the Windows analogue of suppressing macOS App Nap.
// It affects only this process and never the machine's sleep policy.
//
// The API sets a *process-global* policy rather than handing back a per-object
// handle, so concurrent NapInhibitors are reference-counted here. Both the
// count transition and the OS call are done under one mutex, so the policy
// stays consistent with the count under concurrent inhibit/uninhibit: while the
// count is > 0 the opt-out is applied. The call needs Windows 10 1709+; on
// older systems SetProcessInformation() fails and inhibit() returns false.

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0A00  // Windows 10
#endif
#ifndef NTDDI_VERSION
#define NTDDI_VERSION 0x0A000004  // NTDDI_WIN10_RS3 (1709): ProcessPowerThrottling
#endif
#ifndef NOMINMAX
#define NOMINMAX  // keep <windows.h> from defining min()/max() macros that
#endif            // would clobber the STL headers pulled in by backend.hpp

#include <windows.h>

#include <mutex>

#include "backend.hpp"

namespace woke::detail {

namespace {

// Process-global refcount + the mutex that guards both it and the OS call.
// Invariant (held under the mutex): g_nap_refcount > 0 iff the opt-out is
// applied, because the count only rises past 0 after a successful apply.
std::mutex g_nap_mutex;
int g_nap_refcount = 0;

bool apply_throttling_policy(bool opt_out) {
  PROCESS_POWER_THROTTLING_STATE state = {};
  state.Version = PROCESS_POWER_THROTTLING_CURRENT_VERSION;
  state.ControlMask = opt_out ? PROCESS_POWER_THROTTLING_EXECUTION_SPEED : 0;
  state.StateMask = 0;  // 0 => opt out (high performance) / reset to default
  return ::SetProcessInformation(::GetCurrentProcess(), ProcessPowerThrottling,
                                 &state, sizeof(state)) != 0;
}

class WindowsNapBackend final : public Backend {
public:
  ~WindowsNapBackend() override { uninhibit(); }

  bool inhibit(const std::string& /*who*/, const std::string& /*reason*/) override {
    if (active_) return true;
    const std::lock_guard<std::mutex> lock(g_nap_mutex);
    if (g_nap_refcount == 0 && !apply_throttling_policy(true)) {
      return false;  // stay inactive; don't take a reference
    }
    ++g_nap_refcount;
    active_ = true;
    return true;
  }

  void uninhibit() noexcept override {
    if (!active_) return;
    const std::lock_guard<std::mutex> lock(g_nap_mutex);
    active_ = false;
    if (--g_nap_refcount == 0) {
      apply_throttling_policy(false);
    }
  }

  bool active() const noexcept override { return active_; }

private:
  bool active_ = false;
};

}  // namespace

std::unique_ptr<Backend> make_nap_backend() {
  return std::make_unique<WindowsNapBackend>();
}

const char* nap_backend_name() { return "windows"; }

}  // namespace woke::detail
