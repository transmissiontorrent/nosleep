# libwoke

A tiny, dependency-free C++17 library to keep a desktop **awake and responsive**
on Windows, macOS, and Linux. Two independent RAII inhibitors:

- **`woke::SleepInhibitor`**: keep the machine from auto-sleeping (suspend / hibernation).
- **`woke::NapInhibitor`**: keep *this process* from being throttled when idle (macOS "App Nap", Windows power throttling).

Use either or both; each holds an OS request while alive and releases it when destroyed.

## Quick start

```cpp
#include <woke/woke.hpp>

woke::SleepInhibitor sleep;
woke::NapInhibitor nap;

sleep.inhibit("MyApp", "Transferring files");   // keep the machine awake
nap.inhibit("MyApp", "Transferring files");     // keep this process un-throttled

// ... do the work ...
// Both release automatically at end of scope, or call uninhibit().
```

> **Keeps the machine awake, not the screen.** `SleepInhibitor` stops the
> computer from *sleeping* but lets the display turn off and the screensaver
> run (the right behavior for background work like downloads, backups).

## API

Both types have the same shape: movable, not copyable:

| Member | Description |
| --- | --- |
| `bool inhibit(who, reason = …)` | Start inhibiting. `who` names your app, `reason` says why (both shown by the OS). Returns `true` if active. |
| `void uninhibit()` | Release. Safe to call when not active. |
| `bool active()` | Whether a request is held. |
| `static const char* backend_name()` | Diagnostic id of the compiled-in backend. |

- **A `false` from `inhibit()` is best-effort, not an error**: proceed, but don't assume the machine stays awake. It's the *normal* result on Linux without logind (headless, containers, non-systemd) and, for `NapInhibitor`, on Windows older than 10 1709. Only `inhibit()` can throw (on allocation); the rest are `noexcept`.
- **Idempotent:** calling `inhibit()` again while active returns `true` but keeps the original `who`/`reason`. To change them, `uninhibit()` first.
- **Thread safety:** don't call one object's methods concurrently from multiple threads; using *distinct* objects from different threads is fine (the process-global Windows nap policy is internally synchronized).

## Platforms

Each type selects one backend at compile time. Inspect a live sleep inhibitor
with `systemd-inhibit --list` (Linux), `pmset -g assertions` (macOS), or
`powercfg /requests` (Windows).

**`SleepInhibitor`**

| Platform | Mechanism | `backend_name()` |
| --- | --- | --- |
| Linux | logind `Inhibit` (`org.freedesktop.login1`, `what="sleep"`) over the system bus, no `libdbus`; needs systemd-logind or elogind. | `"linux-logind"` |
| macOS | IOKit `IOPMAssertionCreateWithName(…PreventUserIdleSystemSleep)`. | `"macos"` |
| Windows | `PowerCreateRequest` + `PowerSetRequest(PowerRequestSystemRequired)` (Windows 7+). | `"windows"` |

**`NapInhibitor`**

| Platform | Mechanism | `backend_name()` |
| --- | --- | --- |
| macOS | `NSProcessInfo` activity (`NSActivityUserInitiatedAllowingIdleSystemSleep`). | `"macos"` |
| Windows | `SetProcessInformation(ProcessPowerThrottling)` opt-out (Windows 10 1709+); concurrent inhibitors are reference-counted. | `"windows"` |
| Linux | No-op (a normal process isn't app-napped); `inhibit()` still returns `true`. | `"linux-none"` |

## Notes

- **Labels are cosmetic UTF-8.** Keep `who` / `reason` short and NUL-free. An interior NUL or invalid UTF-8 is truncated (macOS/Windows) or makes `inhibit()` return `false` (Linux, which passes it to D-Bus).

## Building

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
```

Vendor it (`add_subdirectory` or `FetchContent`) and link the alias:

```cmake
add_subdirectory(woke)
target_link_libraries(my_app PRIVATE woke::woke)
```

Options `WOKE_BUILD_TESTS` / `WOKE_BUILD_EXAMPLES` (both `ON`). Demo:
`./build/examples/woke_example 15`.

## License

MIT. See [LICENSE](LICENSE).
