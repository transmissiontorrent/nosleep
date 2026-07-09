// A tiny demonstration of the nosleep API.
//
// Usage:
//   nosleep_example [seconds]
//
// Holds a sleep inhibitor for the requested number of seconds (default 10),
// then releases it. Pass 0 to just probe whether inhibition is available.

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <thread>

#include "nosleep/nosleep.hpp"

int main(int argc, char** argv) {
  int seconds = 10;
  if (argc > 1) seconds = std::atoi(argv[1]);

  std::printf("nosleep backend: %s\n", nosleep::Inhibitor::backend_name());

  nosleep::Inhibitor inhibitor;
  if (!inhibitor.inhibit("nosleep_example", "nosleep example is running")) {
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
