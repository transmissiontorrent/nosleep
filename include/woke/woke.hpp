// woke - a tiny, dependency-free library to keep a desktop awake and responsive.
//
// It exposes two RAII types:
//
//   * woke::SleepInhibitor - prevents the machine from entering automatic sleep
//                            (suspend / hibernation) while active.
//   * woke::NapInhibitor   - prevents the OS from throttling *this process* when
//                            it looks idle or backgrounded ("App Nap" on macOS,
//                            power throttling / EcoQoS on Windows). No-op on Linux.
//
// The two are orthogonal: SleepInhibitor is about the whole machine's sleep,
// while NapInhibitor is about this process's scheduling. Destroying an object,
// or calling uninhibit(), restores normal behavior.
//
// Sleep backends:
//   * Linux   - systemd-logind "sleep" inhibitor (org.freedesktop.login1) over D-Bus
//   * Windows - PowerCreateRequest / PowerSetRequest(PowerRequestSystemRequired)
//   * macOS   - IOPMAssertionCreateWithName(kIOPMAssertionTypePreventUserIdleSystemSleep)
//
// Nap backends:
//   * Linux   - none (a normal desktop process is not app-napped); no-op success
//   * Windows - SetProcessInformation(ProcessPowerThrottling) opt-out
//   * macOS   - NSProcessInfo activity (NSActivityUserInitiatedAllowingIdleSystemSleep)
//
// The library has no third-party dependencies; each backend uses only the
// facilities provided by the host operating system.

#ifndef WOKE_WOKE_HPP
#define WOKE_WOKE_HPP

#include <memory>
#include <string>

namespace woke {

namespace detail {
class Backend;  // platform-specific impl in the backend_*.cpp files
}

// The default human-readable reason reported to the OS / desktop environment.
inline constexpr const char* kDefaultReason = "Application requested to stay awake";

// SleepInhibitor prevents the system from going to sleep while active.
class SleepInhibitor {
public:
  SleepInhibitor();
  ~SleepInhibitor();

  SleepInhibitor(SleepInhibitor&&) noexcept;
  SleepInhibitor& operator=(SleepInhibitor&&) noexcept;

  SleepInhibitor(const SleepInhibitor&) = delete;
  SleepInhibitor& operator=(const SleepInhibitor&) = delete;

  // Ask the OS to prevent automatic sleep.
  // Returns true if the request was accepted (and the inhibitor is now active).
  // Calling inhibit() while already active is a no-op that returns true.
  //
  //   who    - a short human-readable identifier for the calling application.
  //   reason - a human-readable explanation of why sleep is being inhibited.
  //
  // How the two strings are used depends on the platform:
  //
  //   Linux   - `who` and `reason` become logind's `who` / `why` fields
  //             (both visible in `systemd-inhibit --list`).
  //   macOS   - combined into the single IOPMAssertion name ("who: reason"),
  //             visible in `pmset -g assertions`.
  //   Windows - combined into the power request's reason string ("who: reason"),
  //             visible in `powercfg /requests`.
  bool inhibit(const std::string& who, const std::string& reason = kDefaultReason);

  // Release the request, allowing the system to sleep again. Safe to call when
  // not active.
  void uninhibit() noexcept;

  // True if a sleep-inhibiting request is currently held.
  [[nodiscard]] bool active() const noexcept;

  // Name of the compiled-in backend ("windows", "macos", or "linux-logind").
  // Useful for diagnostics.
  [[nodiscard]] static const char* backend_name() noexcept;

private:
  std::unique_ptr<detail::Backend> impl_;
};

// NapInhibitor keeps the current process from being throttled or "napped" by
// the OS when it appears idle or backgrounded, so background work (network I/O,
// timers) keeps running at full speed. This concerns only *this process*; it
// does NOT keep the machine awake -- pair it with a SleepInhibitor for that.
//
//   macOS   - holds an NSProcessInfo activity with
//             NSActivityUserInitiatedAllowingIdleSystemSleep, suppressing App
//             Nap while still allowing normal system sleep.
//   Windows - clears PROCESS_POWER_THROTTLING_EXECUTION_SPEED via
//             SetProcessInformation(), opting the process out of power throttling
//             (Windows 10 1709+; a no-op that returns false on older systems).
//   Linux   - no-op: a normal desktop process is not app-napped. inhibit()
//             reports success so callers can treat all platforms uniformly.
//
// Same shape as SleepInhibitor: movable, not copyable; released by the destructor.
class NapInhibitor {
public:
  NapInhibitor();
  ~NapInhibitor();

  NapInhibitor(NapInhibitor&&) noexcept;
  NapInhibitor& operator=(NapInhibitor&&) noexcept;

  NapInhibitor(const NapInhibitor&) = delete;
  NapInhibitor& operator=(const NapInhibitor&) = delete;

  // Ask the OS not to throttle / nap this process. Returns true if active.
  // Idempotent. `who` / `reason` are surfaced where the platform supports it
  // (the macOS activity reason) and ignored elsewhere.
  bool inhibit(const std::string& who, const std::string& reason = kDefaultReason);

  // Allow normal throttling again. Safe to call when not active.
  void uninhibit() noexcept;

  // True if a nap-inhibiting request is currently held.
  [[nodiscard]] bool active() const noexcept;

  // Name of the compiled-in nap backend ("macos", "windows", or "linux-none").
  [[nodiscard]] static const char* backend_name() noexcept;

private:
  std::unique_ptr<detail::Backend> impl_;
};

}  // namespace woke

#endif  // WOKE_WOKE_HPP
