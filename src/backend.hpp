// Internal backend interface. Each platform provides a concrete Backend and an
// implementation of make_backend(). This header is not installed and is not
// part of the public API.

#ifndef WOKE_SRC_BACKEND_HPP
#define WOKE_SRC_BACKEND_HPP

#include <memory>
#include <string>

namespace woke::detail {

class Backend {
public:
  virtual ~Backend() = default;

  // Begin inhibiting sleep. Returns true on success.
  // Implementations must be idempotent: calling inhibit() while already active
  // returns true without creating a second OS request.
  // `who` identifies the application;
  // `reason` explains the request (used where the platform supports it).
  virtual bool inhibit(const std::string& who, const std::string& reason) = 0;

  // Release any active request. Must be safe to call when inactive.
  virtual void uninhibit() noexcept = 0;

  virtual bool active() const noexcept = 0;
};

[[nodiscard]] std::unique_ptr<Backend> make_backend();

// backend_name() returns a static id ("windows" / "macos" / "linux-logind").
[[nodiscard]] const char* backend_name();

// Fold an application identity and a reason into a single human-readable label
// ("who: reason"), for the platforms that expose only one string slot.
[[nodiscard]] inline std::string combine_label(const std::string& who,
                                               const std::string& reason) {
  std::string label = who;
  if (!who.empty() && !reason.empty()) label += ": ";
  label += reason;
  if (label.empty()) label = "woke";
  return label;
}

}  // namespace woke::detail

#endif  // WOKE_SRC_BACKEND_HPP
