#include "woke/woke.hpp"

#include <utility>

#include "backend.hpp"

namespace woke {

// ---- SleepInhibitor (system sleep) ---------------------------------------

SleepInhibitor::SleepInhibitor() : impl_(detail::make_sleep_backend()) {}

// Out-of-line so that detail::Backend need only be complete here, keeping the
// public header free of platform details. The backend's own destructor is
// responsible for releasing any held request.
SleepInhibitor::~SleepInhibitor() = default;

SleepInhibitor::SleepInhibitor(SleepInhibitor&&) noexcept = default;
SleepInhibitor& SleepInhibitor::operator=(SleepInhibitor&&) noexcept = default;

bool SleepInhibitor::inhibit(const std::string& who, const std::string& reason) {
  return impl_ ? impl_->inhibit(who, reason) : false;
}

void SleepInhibitor::uninhibit() noexcept {
  if (impl_) impl_->uninhibit();
}

bool SleepInhibitor::active() const noexcept {
  return impl_ && impl_->active();
}

const char* SleepInhibitor::backend_name() noexcept {
  return detail::sleep_backend_name();
}

// ---- NapInhibitor (process throttling / App Nap) -------------------------

NapInhibitor::NapInhibitor() : impl_(detail::make_nap_backend()) {}

NapInhibitor::~NapInhibitor() = default;

NapInhibitor::NapInhibitor(NapInhibitor&&) noexcept = default;
NapInhibitor& NapInhibitor::operator=(NapInhibitor&&) noexcept = default;

bool NapInhibitor::inhibit(const std::string& who, const std::string& reason) {
  return impl_ ? impl_->inhibit(who, reason) : false;
}

void NapInhibitor::uninhibit() noexcept {
  if (impl_) impl_->uninhibit();
}

bool NapInhibitor::active() const noexcept {
  return impl_ && impl_->active();
}

const char* NapInhibitor::backend_name() noexcept {
  return detail::nap_backend_name();
}

}  // namespace woke
