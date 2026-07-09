#include "nosleep/nosleep.hpp"

#include <utility>

#include "backend.hpp"

namespace nosleep {

Inhibitor::Inhibitor() : impl_(detail::make_backend()) {}

// Out-of-line so that detail::Backend need only be complete here, keeping the
// public header free of platform details. The backend's own destructor is
// responsible for releasing any held request.
Inhibitor::~Inhibitor() = default;

Inhibitor::Inhibitor(Inhibitor&&) noexcept = default;
Inhibitor& Inhibitor::operator=(Inhibitor&&) noexcept = default;

bool Inhibitor::inhibit(const std::string& who, const std::string& reason) {
  return impl_ ? impl_->inhibit(who, reason) : false;
}

void Inhibitor::uninhibit() noexcept {
  if (impl_) impl_->uninhibit();
}

bool Inhibitor::active() const noexcept {
  return impl_ && impl_->active();
}

const char* Inhibitor::backend_name() noexcept {
  return detail::backend_name();
}

}  // namespace nosleep
