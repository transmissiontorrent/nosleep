# woke

A tiny, dependency-free C++ library to keep a desktop **awake and responsive**.
It offers two independent RAII inhibitors:

- **`woke::SleepInhibitor`** — prevents the machine from entering automatic
  sleep / suspend / hibernation.
- **`woke::NapInhibitor`** — prevents the OS from throttling *this process* when
  it looks idle or backgrounded (macOS "App Nap", Windows power throttling).

The two are orthogonal: one is about the whole machine's sleep, the other about
this process's scheduling. Use either or both.

- **No third-party dependencies.** Each backend uses only what the operating
  system already provides.
- **Cross-platform:** Windows, macOS, and Linux.
- **CMake + CTest**, with CI on GitHub-hosted Windows, macOS, and Linux runners.

## API

```cpp
#include <woke/woke.hpp>

int main() {
    woke::SleepInhibitor sleep;  // keep the machine awake
    woke::NapInhibitor nap;      // keep this process un-throttled

    if (sleep.inhibit("MyApp", "Transferring files")) {
        nap.inhibit("MyApp", "Transferring files");
        // ... do work that should not be interrupted ...
    }

    // Both release automatically at end of scope (or call uninhibit()).
}
```

Both types share the same shape:

| Member | Description |
| --- | --- |
| `bool inhibit(who, reason = ...)` | Ask the OS to inhibit. `who` identifies your app; `reason` explains why. Returns `true` if active. Idempotent. |
| `void uninhibit() noexcept` | Release the request. Safe to call when inactive. |
| `bool active() const noexcept` | Whether a request is currently held. |
| `static const char* backend_name()` | The compiled-in backend id (see below). |

Both are movable but not copyable; the OS request follows the object and is
released automatically by the destructor.

### `woke::SleepInhibitor` — system sleep

| Platform | Mechanism | `backend_name()` |
| --- | --- | --- |
| Windows | [`PowerCreateRequest`](https://learn.microsoft.com/windows/win32/api/winbase/nf-winbase-powercreaterequest) + `PowerSetRequest(PowerRequestSystemRequired)`; visible in `powercfg /requests`. | `"windows"` |
| macOS | IOKit [`IOPMAssertionCreateWithName`](https://developer.apple.com/documentation/iokit) (`kIOPMAssertionTypePreventUserIdleSystemSleep`); visible in `pmset -g assertions`. | `"macos"` |
| Linux | The D-Bus wire protocol directly over the **system** bus (no `libdbus`), calling [`org.freedesktop.login1.Manager.Inhibit`](https://www.freedesktop.org/software/systemd/man/latest/org.freedesktop.login1.html) with `what="sleep"`, `mode="block"`. | `"linux-logind"` |

`who` and `reason` are surfaced per platform: Linux uses them as logind's
separate `who` / `why` fields; macOS and Windows combine them into a single
`"who: reason"` string. Inhibition prevents automatic suspend / hibernation
while **leaving the screensaver and display-blanking untouched** — the right
behaviour for a background task such as a network transfer.

On Linux, logind returns a file descriptor and holds the lock until it is
closed, served by systemd-logind or the compatible `elogind`; if neither is
reachable, `inhibit()` returns `false`. See an active inhibitor with
`systemd-inhibit --list`.

### `woke::NapInhibitor` — process throttling / App Nap

Keeps *this process* from being throttled or "napped" when it appears idle or
backgrounded, so background work (network I/O, timers) keeps running at full
speed. It never affects the machine's sleep policy — pair it with a
`SleepInhibitor` for that.

| Platform | Mechanism | `backend_name()` |
| --- | --- | --- |
| macOS | An `NSProcessInfo` activity with `NSActivityUserInitiatedAllowingIdleSystemSleep`, suppressing App Nap while still allowing normal system sleep. | `"macos"` |
| Windows | `SetProcessInformation(ProcessPowerThrottling)` clearing `PROCESS_POWER_THROTTLING_EXECUTION_SPEED` — opts the process out of power throttling (EcoQoS). Requires Windows 10 1709+ (older systems fail gracefully at `inhibit()`). | `"windows"` |
| Linux | No-op — a normal desktop process is not app-napped. `inhibit()` reports success so callers can treat every platform uniformly. | `"linux-none"` |

On Windows the throttling policy is process-global, so concurrent
`NapInhibitor`s are reference-counted: the opt-out is applied on the first and
cleared on the last.

## Building

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
```

### Options

| Option | Default | Description |
| --- | --- | --- |
| `WOKE_BUILD_TESTS` | `ON` | Build the CTest suite. |
| `WOKE_BUILD_EXAMPLES` | `ON` | Build the `woke_example` demo. |

### Using it from CMake

`woke` can be vendored (e.g. via `add_subdirectory` or `FetchContent`) and
linked through the exported alias:

```cmake
add_subdirectory(woke)
target_link_libraries(my_app PRIVATE woke::woke)
```

## Try the example

```sh
./build/examples/woke_example 15   # stay awake & responsive for 15 seconds
```

## License

MIT — see [LICENSE](LICENSE).
