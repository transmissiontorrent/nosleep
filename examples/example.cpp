// A tiny demonstration of the woke API.
//
// Usage:
//   woke_example [seconds]
//
// Holds a sleep inhibitor *and* a nap inhibitor for the requested number of
// seconds (default 10), then releases them. Pass 0 to just probe availability.

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <thread>

#include "woke/woke.hpp"

int main(int argc, char** argv) {
  int seconds = 10;
  if (argc > 1) seconds = std::atoi(argv[1]);

  std::printf("woke sleep backend: %s\n", woke::SleepInhibitor::backend_name());
  std::printf("woke nap backend:   %s\n", woke::NapInhibitor::backend_name());

  woke::SleepInhibitor sleep;
  if (!sleep.inhibit("woke_example", "woke example is running")) {
    std::printf("Could not inhibit sleep in this environment.\n");
    return 1;
  }

  // Keep this process responsive (un-throttled) while it works. This is a no-op
  // on Linux, so ignore its return value.
  woke::NapInhibitor nap;
  nap.inhibit("woke_example", "woke example is running");

  std::printf("Sleep inhibited (active=%s), nap inhibited (active=%s).\n",
              sleep.active() ? "true" : "false",
              nap.active() ? "true" : "false");

  for (int remaining = seconds; remaining > 0; --remaining) {
    std::printf("\r  staying awake for %d more second(s)... ", remaining);
    std::fflush(stdout);
    std::this_thread::sleep_for(std::chrono::seconds(1));
  }
  std::printf("\n");

  nap.uninhibit();
  sleep.uninhibit();
  std::printf("Released (sleep active=%s, nap active=%s).\n",
              sleep.active() ? "true" : "false",
              nap.active() ? "true" : "false");
  return 0;
}
