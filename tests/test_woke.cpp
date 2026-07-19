// Public-API tests for woke, driven by CTest. Each test case is selected by
// the first command-line argument; the process exits non-zero on failure.
//
// SleepInhibitor and NapInhibitor share an identical shape, so the common
// lifecycle checks are written once as function templates and instantiated for
// each type from the dispatch table in main().

#include <cstdio>
#include <cstring>
#include <utility>

#include "woke/woke.hpp"
#include "test_util.hpp"

namespace {

// SleepInhibitor's OS call is expected to succeed on Windows and macOS even in
// a headless CI environment. On Linux success needs a reachable logind, so the
// sleep lifecycle tolerates "unavailable".
#if defined(_WIN32) || defined(__APPLE__)
constexpr bool kSleepMustSucceed = true;
#else
constexpr bool kSleepMustSucceed = false;
#endif

// The nap inhibitor is a no-op success on Linux and reliable on macOS. On
// Windows it needs Windows 10 1709+, so tolerate an "unavailable" result.
#if defined(_WIN32)
constexpr bool kNapMustSucceed = false;
#else
constexpr bool kNapMustSucceed = true;
#endif

template <class Inhibitor>
void test_backend_name() {
  const char* name = Inhibitor::backend_name();
  CHECK(name != nullptr);
  CHECK(std::strcmp(name, "none") != 0);
  std::printf("  backend: %s\n", name);
}

template <class Inhibitor>
void test_construct_destruct() {
  Inhibitor inhibitor;
  CHECK(!inhibitor.active());
}

template <class Inhibitor>
void test_uninhibit_without_inhibit() {
  Inhibitor inhibitor;
  inhibitor.uninhibit();  // must be a safe no-op
  CHECK(!inhibitor.active());
}

template <class Inhibitor>
void test_lifecycle(bool must_succeed) {
  Inhibitor inhibitor;
  const bool ok = inhibitor.inhibit("woke_tests", "woke test");

  if (must_succeed) {
    CHECK(ok);
    CHECK(inhibitor.active());
  } else if (!ok) {
    std::printf("  inhibit unavailable in this environment; skipping\n");
    CHECK(!inhibitor.active());
  } else {
    CHECK(inhibitor.active());
  }

  inhibitor.uninhibit();
  CHECK(!inhibitor.active());
}

template <class Inhibitor>
void test_reinhibit(bool must_succeed) {
  Inhibitor inhibitor;
  const bool first = inhibitor.inhibit("woke_tests", "first");
  inhibitor.uninhibit();
  CHECK(!inhibitor.active());
  const bool second = inhibitor.inhibit("woke_tests", "second");

  if (must_succeed) {
    CHECK(first);
    CHECK(second);
    CHECK(inhibitor.active());
  }
  // Destructor must release cleanly regardless.
}

template <class Inhibitor>
void test_idempotent(bool must_succeed) {
  Inhibitor inhibitor;
  const bool first = inhibitor.inhibit("woke_tests", "first");
  const bool second = inhibitor.inhibit("woke_tests", "second");  // while active

  CHECK(second == first);  // a second inhibit while active repeats the result
  if (must_succeed) {
    CHECK(inhibitor.active());
  }

  inhibitor.uninhibit();  // one release must fully release (no second request)
  CHECK(!inhibitor.active());
}

template <class Inhibitor>
void test_move_semantics() {
  Inhibitor source;
  source.inhibit("woke_tests", "move");
  const bool was_active = source.active();

  Inhibitor moved(std::move(source));
  CHECK(moved.active() == was_active);

  source.uninhibit();  // moved-from object must be safe to use
  CHECK(!source.active());

  moved.uninhibit();
  CHECK(!moved.active());
}

template <class Inhibitor>
void test_move_assign() {
  Inhibitor source;
  source.inhibit("woke_tests", "source");
  const bool was_active = source.active();

  Inhibitor target;
  target.inhibit("woke_tests", "target");  // target holds its own resource
  target = std::move(source);              // must release target's old resource
  CHECK(target.active() == was_active);

  source.uninhibit();  // moved-from object stays safe to use
  CHECK(!source.active());
  target.uninhibit();
  CHECK(!target.active());
}

// Nap inhibitors have independent lifetimes: releasing one must not disturb
// another's held state. (The Windows backend shares a process-global policy via
// a refcount, but that policy isn't observable from userspace, so this asserts
// the wrapper-level invariant we can see.)
void test_nap_independence() {
  woke::NapInhibitor a;
  woke::NapInhibitor b;
  a.inhibit("woke_tests", "a");
  b.inhibit("woke_tests", "b");

  const bool b_active = b.active();
  a.uninhibit();
  CHECK(b.active() == b_active);  // releasing a leaves b unchanged

  b.uninhibit();
  CHECK(!b.active());
}

}  // namespace

int main(int argc, char** argv) {
  using Sleep = woke::SleepInhibitor;
  using Nap = woke::NapInhibitor;
  return run_test(argc, argv, {
      {"sleep_backend_name", [] { test_backend_name<Sleep>(); }},
      {"sleep_construct_destruct", [] { test_construct_destruct<Sleep>(); }},
      {"sleep_uninhibit_without_inhibit", [] { test_uninhibit_without_inhibit<Sleep>(); }},
      {"sleep_lifecycle", [] { test_lifecycle<Sleep>(kSleepMustSucceed); }},
      {"sleep_reinhibit", [] { test_reinhibit<Sleep>(kSleepMustSucceed); }},
      {"sleep_idempotent", [] { test_idempotent<Sleep>(kSleepMustSucceed); }},
      {"sleep_move_semantics", [] { test_move_semantics<Sleep>(); }},
      {"sleep_move_assign", [] { test_move_assign<Sleep>(); }},
      {"nap_backend_name", [] { test_backend_name<Nap>(); }},
      {"nap_construct_destruct", [] { test_construct_destruct<Nap>(); }},
      {"nap_uninhibit_without_inhibit", [] { test_uninhibit_without_inhibit<Nap>(); }},
      {"nap_lifecycle", [] { test_lifecycle<Nap>(kNapMustSucceed); }},
      {"nap_reinhibit", [] { test_reinhibit<Nap>(kNapMustSucceed); }},
      {"nap_idempotent", [] { test_idempotent<Nap>(kNapMustSucceed); }},
      {"nap_move_semantics", [] { test_move_semantics<Nap>(); }},
      {"nap_move_assign", [] { test_move_assign<Nap>(); }},
      {"nap_independence", [] { test_nap_independence(); }},
  });
}
