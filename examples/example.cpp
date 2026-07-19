// A tiny demonstration of the woke API.
//
// Usage:
//   woke_example [seconds]
//
// Holds a sleep inhibitor for the requested number of seconds (default 10),
// then releases it. Pass 0 to just probe whether inhibition is available.

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <thread>

#include "woke/woke.hpp"

int main(int argc, char** argv) {
  int seconds = 10;
  if (argc > 1) seconds = std::atoi(argv[1]);

  std::printf("woke backend: %s\n", woke::Inhibitor::backend_name());

  woke::Inhibitor inhibitor;
  if (!inhibitor.inhibit("woke_example", "woke example is running")) {
    std::printf("Could not inhibit sleep in this environment.\n");
    return 1;
  }

  std::printf("Sleep is inhibited (active=%s).\n",
              inhibitor.active() ? "true" : "false");

  for (int remaining = seconds; remaining > 0; --remaining) {
    std::printf("\r  staying awake for %d more second(s)... ", remaining);
    std::fflush(stdout);
    std::this_thread::sleep_for(std::chrono::seconds(1));
  }
  std::printf("\n");

  inhibitor.uninhibit();
  std::printf("Sleep inhibitor released (active=%s).\n",
              inhibitor.active() ? "true" : "false");
  return 0;
}
