// Public-API tests for woke, driven by CTest. Each test case is selected by
// the first command-line argument; the process exits non-zero on failure.

#include <cstdio>
#include <cstring>
#include <string>
#include <utility>

#include "woke/woke.hpp"
#include "test_util.hpp"

namespace {

// On Windows and macOS the OS call is expected to succeed even in a headless
// CI environment. On Linux success depends on a reachable inhibitor service, so
// the lifecycle test tolerates "unavailable".
#if defined(_WIN32) || defined(__APPLE__)
constexpr bool kInhibitMustSucceed = true;
#else
constexpr bool kInhibitMustSucceed = false;
#endif

void test_backend_name() {
  const char* name = woke::Inhibitor::backend_name();
  CHECK(name != nullptr);
  CHECK(std::strcmp(name, "none") != 0);
  std::printf("  backend: %s\n", name);
}

void test_construct_destruct() {
  woke::Inhibitor inhibitor;
  CHECK(!inhibitor.active());
}

void test_uninhibit_without_inhibit() {
  woke::Inhibitor inhibitor;
  inhibitor.uninhibit();  // must be a safe no-op
  CHECK(!inhibitor.active());
}

void test_inhibit_lifecycle() {
  woke::Inhibitor inhibitor;
  const bool ok = inhibitor.inhibit("woke_tests", "woke test");

  if (kInhibitMustSucceed) {
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

void test_reinhibit() {
  woke::Inhibitor inhibitor;
  const bool first = inhibitor.inhibit("woke_tests", "first");
  inhibitor.uninhibit();
  CHECK(!inhibitor.active());
  const bool second = inhibitor.inhibit("woke_tests", "second");

  if (kInhibitMustSucceed) {
    CHECK(first);
    CHECK(second);
    CHECK(inhibitor.active());
  }
  // Destructor must release cleanly regardless.
}

void test_move_semantics() {
  woke::Inhibitor source;
  source.inhibit("woke_tests", "move");
  const bool was_active = source.active();

  woke::Inhibitor moved(std::move(source));
  CHECK(moved.active() == was_active);

  // The moved-from object must be safe to use.
  source.uninhibit();
  CHECK(!source.active());

  moved.uninhibit();
  CHECK(!moved.active());
}

}  // namespace

int main(int argc, char** argv) {
  if (argc < 2) {
    std::printf("usage: %s <case>\n", argv[0]);
    return 2;
  }
  const std::string test = argv[1];

  if (test == "backend_name") {
    test_backend_name();
  } else if (test == "construct_destruct") {
    test_construct_destruct();
  } else if (test == "uninhibit_without_inhibit") {
    test_uninhibit_without_inhibit();
  } else if (test == "inhibit_lifecycle") {
    test_inhibit_lifecycle();
  } else if (test == "reinhibit") {
    test_reinhibit();
  } else if (test == "move_semantics") {
    test_move_semantics();
  } else {
    std::printf("unknown test case: %s\n", test.c_str());
    return 2;
  }

  return test_report(test.c_str());
}
