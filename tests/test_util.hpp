// Minimal shared test harness for the woke CTest executables. Each test case
// is selected by argv[1]; CHECK records a failure; run_test() dispatches to the
// named case and test_report() yields the process exit code.

#ifndef WOKE_TESTS_TEST_UTIL_HPP
#define WOKE_TESTS_TEST_UTIL_HPP

#include <cstdio>
#include <cstring>
#include <initializer_list>

inline int g_failures = 0;

#define CHECK(cond)                                                     \
  do {                                                                  \
    if (!(cond)) {                                                      \
      std::printf("  CHECK failed: %s  (%s:%d)\n", #cond, __FILE__,     \
                  __LINE__);                                            \
      ++g_failures;                                                     \
    }                                                                   \
  } while (0)

inline int test_report(const char* name) {
  if (g_failures != 0) {
    std::printf("[FAIL] %s: %d check(s) failed\n", name, g_failures);
    return 1;
  }
  std::printf("[ OK ] %s\n", name);
  return 0;
}

// Shared main() dispatcher: each test binary lists its cases as {name, fn}
// pairs; run_test() runs the one named by argv[1] and reports the result.
struct TestCase {
  const char* name;
  void (*fn)();
};

inline int run_test(int argc, char** argv, std::initializer_list<TestCase> cases) {
  if (argc < 2) {
    std::printf("usage: %s <case>\n", argv[0]);
    return 2;
  }
  for (const TestCase& c : cases) {
    if (std::strcmp(argv[1], c.name) == 0) {
      c.fn();
      return test_report(c.name);
    }
  }
  std::printf("unknown test case: %s\n", argv[1]);
  return 2;
}

#endif  // WOKE_TESTS_TEST_UTIL_HPP
