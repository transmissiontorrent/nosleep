// Minimal shared test harness for the woke CTest executables. Each test case
// is selected by argv[1]; CHECK records a failure; test_report() prints the
// result and yields the process exit code.

#ifndef WOKE_TESTS_TEST_UTIL_HPP
#define WOKE_TESTS_TEST_UTIL_HPP

#include <cstdio>

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

#endif  // WOKE_TESTS_TEST_UTIL_HPP
