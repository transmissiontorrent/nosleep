// macOS backend: IOKit power-management assertions.
//
// kIOPMAssertionTypePreventUserIdleSystemSleep prevents the system from
// sleeping when the user is idle, which is the desktop "keep awake" behaviour.
// The assertion is released with IOPMAssertionRelease.

#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/pwr_mgt/IOPMLib.h>

#include "backend.hpp"

namespace woke::detail {

namespace {

class MacBackend final : public Backend {
public:
  ~MacBackend() override { uninhibit(); }

  bool inhibit(const std::string& who, const std::string& reason) override {
    if (active_) return true;

    // IOPMAssertionCreateWithName exposes a single "name" string, so fold the
    // application identity and the reason into one label.
    const std::string name = combine_label(who, reason);

    CFStringRef cf_name = CFStringCreateWithCString(
        kCFAllocatorDefault, name.c_str(), kCFStringEncodingUTF8);
    if (cf_name == nullptr) return false;

    const IOReturn result = IOPMAssertionCreateWithName(
        kIOPMAssertionTypePreventUserIdleSystemSleep, kIOPMAssertionLevelOn,
        cf_name, &assertion_);
    CFRelease(cf_name);

    active_ = (result == kIOReturnSuccess);
    return active_;
  }

  void uninhibit() noexcept override {
    if (!active_) return;
    IOPMAssertionRelease(assertion_);
    assertion_ = kIOPMNullAssertionID;
    active_ = false;
  }

  bool active() const noexcept override { return active_; }

private:
  IOPMAssertionID assertion_ = kIOPMNullAssertionID;
  bool active_ = false;
};

}  // namespace

std::unique_ptr<Backend> make_backend() {
  return std::make_unique<MacBackend>();
}

const char* backend_name() { return "macos"; }

}  // namespace woke::detail
