// macOS nap backend: suppress App Nap via an NSProcessInfo activity.
//
// NSActivityUserInitiatedAllowingIdleSystemSleep marks the process as doing
// user-initiated work -- so the OS will not App-Nap / throttle it (nor terminate
// it abruptly) -- while *allowing* normal idle system sleep. That keeps this
// concern orthogonal to woke::SleepInhibitor, which handles machine sleep.
//
// -beginActivityWithOptions:reason: returns an autoreleased token that must be
// retained for as long as the activity should last. This file is compiled
// without ARC (see CMakeLists.txt), so the token is managed with explicit
// retain / release.

#import <Foundation/Foundation.h>

#include "backend.hpp"

namespace woke::detail {

namespace {

class MacNapBackend final : public Backend {
public:
  ~MacNapBackend() override { uninhibit(); }

  bool inhibit(const std::string& who, const std::string& reason) override {
    if (active_) return true;

    @autoreleasepool {
      NSString* ns_reason =
          [NSString stringWithUTF8String:combine_label(who, reason).c_str()];
      if (ns_reason == nil) ns_reason = @"woke";

      id token = [[NSProcessInfo processInfo]
          beginActivityWithOptions:NSActivityUserInitiatedAllowingIdleSystemSleep
                            reason:ns_reason];
      if (token == nil) return false;

      token_ = [token retain];  // survive the autorelease pool drain
    }

    active_ = true;
    return true;
  }

  void uninhibit() noexcept override {
    if (!active_) return;
    [[NSProcessInfo processInfo] endActivity:static_cast<id>(token_)];
    [static_cast<id>(token_) release];
    token_ = nullptr;
    active_ = false;
  }

  bool active() const noexcept override { return active_; }

private:
  void* token_ = nullptr;  // a retained id<NSObject>
  bool active_ = false;
};

}  // namespace

std::unique_ptr<Backend> make_nap_backend() {
  return std::make_unique<MacNapBackend>();
}

const char* nap_backend_name() { return "macos"; }

}  // namespace woke::detail
