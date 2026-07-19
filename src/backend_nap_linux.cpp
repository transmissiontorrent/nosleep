// Linux nap backend: no-op.
//
// A normal desktop Linux process is not subject to an "App Nap"-style
// background throttle the way macOS / Windows processes are, so there is
// nothing to opt out of. inhibit() therefore succeeds trivially and holds no OS
// resource, letting cross-platform callers treat every platform uniformly.
//
// (A future version could raise the process timer-slack via
// prctl(PR_SET_TIMERSLACK) should cgroup-based desktop throttling ever make it
// worthwhile.)

#include "backend.hpp"

namespace woke::detail {

namespace {

class LinuxNapBackend final : public Backend {
public:
  bool inhibit(const std::string& /*who*/, const std::string& /*reason*/) override {
    active_ = true;
    return true;
  }

  void uninhibit() noexcept override { active_ = false; }

  bool active() const noexcept override { return active_; }

private:
  bool active_ = false;
};

}  // namespace

std::unique_ptr<Backend> make_nap_backend() {
  return std::make_unique<LinuxNapBackend>();
}

const char* nap_backend_name() { return "linux-none"; }

}  // namespace woke::detail
