// nosleep - a tiny, dependency-free library to keep a desktop machine awake.
//
// The library exposes a single RAII type, nosleep::Inhibitor.
// While an inhibitor is active, the OS is asked not to enter automatic sleep.
// Destroying the inhibitor or calling uninhibit() restores normal behavior.
//
// Backends:
//   * Linux   - systemd-logind "sleep" inhibitor (org.freedesktop.login1) over D-Bus
//   * Windows - PowerCreateRequest / PowerSetRequest(PowerRequestSystemRequired)
//   * macOS   - IOPMAssertionCreateWithName(kIOPMAssertionTypePreventUserIdleSystemSleep)
//
// The inhibition prevents automatic suspend / hibernation but deliberately does
// NOT keep the display awake or block the screensaver.
//
// The library has no third-party dependencies; each backend uses only the
// facilities provided by the host operating system.

#ifndef NOSLEEP_NOSLEEP_HPP
#define NOSLEEP_NOSLEEP_HPP

#include <memory>
#include <string>

namespace nosleep {

namespace detail {
class Backend;  // platform-specific impl in the backend_*.cpp files
}

// The default human-readable reason reported to the OS / desktop environment.
inline constexpr const char* kDefaultReason = "Application requested to stay awake";

// Inhibitor prevents the system from going to sleep while active.
class Inhibitor {
public:
  Inhibitor();
  ~Inhibitor();

  Inhibitor(Inhibitor&&) noexcept;
  Inhibitor& operator=(Inhibitor&&) noexcept;

  Inhibitor(const Inhibitor&) = delete;
  Inhibitor& operator=(const Inhibitor&) = delete;

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

}  // namespace nosleep

#endif  // NOSLEEP_NOSLEEP_HPP
