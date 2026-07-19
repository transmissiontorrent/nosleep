// woke - a tiny, dependency-free library to keep a desktop awake and responsive.
//
// Two RAII types, each holding one OS request while alive and releasing it on
// destruction (or uninhibit()):
//
//   * woke::SleepInhibitor - keep the *machine* from auto-sleeping (suspend /
//                            hibernation). It deliberately lets the display
//                            blank and the screensaver run.
//   * woke::NapInhibitor   - keep *this process* from being throttled or
//                            "napped" when it looks idle (macOS App Nap, Windows
//                            power throttling). No-op on Linux.
//
// Each selects one backend per platform at compile time (see backend_name() and
// the README). No third-party dependencies.
//
// Thread safety: a single object is not thread-safe -- don't call its methods
// concurrently on the same object. Distinct objects may be used concurrently
// from different threads.

#ifndef WOKE_WOKE_HPP
#define WOKE_WOKE_HPP

#include <memory>
#include <string>

namespace woke {

namespace detail {
class Backend;  // platform-specific impl in the backend_*.cpp files
}

// SleepInhibitor prevents the system from going to sleep while active. Movable,
// not copyable.
class SleepInhibitor {
public:
  SleepInhibitor();
  ~SleepInhibitor();

  SleepInhibitor(SleepInhibitor&&) noexcept;
  SleepInhibitor& operator=(SleepInhibitor&&) noexcept;

  SleepInhibitor(const SleepInhibitor&) = delete;
  SleepInhibitor& operator=(const SleepInhibitor&) = delete;

  // Ask the OS to prevent automatic sleep. Returns true if now active; false if
  // the OS mechanism is unavailable (e.g. no logind on Linux) -- treat false as
  // best-effort, not an error. Only inhibit() may throw (on allocation);
  // uninhibit()/active()/backend_name() are noexcept.
  //
  //   who    - a short identifier for your app (e.g. "MyApp").
  //   reason - why you're inhibiting; shown by the OS (systemd-inhibit --list,
  //            pmset -g assertions, powercfg /requests).
  //
  // Idempotent: calling inhibit() while already active returns true and keeps
  // the existing who/reason -- to change them, uninhibit() first.
  bool inhibit(const std::string& who,
               const std::string& reason = "Application requested to stay awake");

  // Release the request, allowing the system to sleep again. Safe to call when
  // not active.
  void uninhibit() noexcept;

  // True if a sleep-inhibiting request is currently held.
  [[nodiscard]] bool active() const noexcept;

  // Diagnostic id of the compiled-in backend ("windows", "macos", "linux-logind").
  [[nodiscard]] static const char* backend_name() noexcept;

private:
  std::unique_ptr<detail::Backend> impl_;
};

// NapInhibitor keeps *this process* running at full speed when it looks idle or
// backgrounded, so background work (I/O, timers) isn't throttled. It does NOT
// keep the machine awake -- pair it with a SleepInhibitor for that. No-op on
// Linux; needs Windows 10 1709+ (older Windows returns false). Movable, not
// copyable.
class NapInhibitor {
public:
  NapInhibitor();
  ~NapInhibitor();

  NapInhibitor(NapInhibitor&&) noexcept;
  NapInhibitor& operator=(NapInhibitor&&) noexcept;

  NapInhibitor(const NapInhibitor&) = delete;
  NapInhibitor& operator=(const NapInhibitor&) = delete;

  // Ask the OS not to throttle this process. Returns true if now active (a no-op
  // success on Linux); false if unavailable. Idempotent, like SleepInhibitor.
  // who/reason are used where the platform surfaces them (the macOS activity
  // reason) and ignored elsewhere.
  bool inhibit(const std::string& who,
               const std::string& reason = "Application requested to stay awake");

  // Allow normal throttling again. Safe to call when not active.
  void uninhibit() noexcept;

  // True if a nap-inhibiting request is currently held.
  [[nodiscard]] bool active() const noexcept;

  // Diagnostic id of the compiled-in backend ("macos", "windows", "linux-none").
  [[nodiscard]] static const char* backend_name() noexcept;

private:
  std::unique_ptr<detail::Backend> impl_;
};

}  // namespace woke

#endif  // WOKE_WOKE_HPP
