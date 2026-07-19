# woke

A tiny, dependency-free C++ library to keep a desktop machine awake — it inhibits
automatic sleep / suspend / hibernation and releases the inhibition on request.

- **No third-party dependencies.** Each backend uses only what the operating
  system already provides.
- **Cross-platform:** Windows, macOS, and Linux.
- **CMake + CTest**, with CI on GitHub-hosted Windows, macOS, and Linux runners.

## API

The entire public API is the RAII type `woke::Inhibitor`:

```cpp
#include <woke/woke.hpp>

int main() {
    woke::Inhibitor inhibitor;

    if (inhibitor.inhibit("MyApp", "Encoding a video")) {
        // ... do work that should not be interrupted by sleep ...
    }

    inhibitor.uninhibit();   // or just let `inhibitor` go out of scope
}
```

| Member | Description |
| --- | --- |
| `bool inhibit(who, reason = ...)` | Ask the OS to prevent automatic sleep. `who` identifies your app; `reason` explains why. Returns `true` if active. Idempotent. |
| `void uninhibit() noexcept` | Release the request. Safe to call when inactive. |
| `bool active() const noexcept` | Whether a request is currently held. |
| `static const char* backend_name()` | `"windows"`, `"macos"`, or `"linux-logind"`. |

`who` and `reason` are surfaced differently per platform: Linux uses them as
logind's separate `who` / `why` fields; macOS and Windows combine them into a
single `"who: reason"` string — the IOPMAssertion name and the power-request
reason respectively.

`Inhibitor` is movable but not copyable; the OS request follows the object and
is released automatically by the destructor.

## How it works

| Platform | Mechanism |
| --- | --- |
| Windows | [`PowerCreateRequest`](https://learn.microsoft.com/windows/win32/api/winbase/nf-winbase-powercreaterequest) + `PowerSetRequest(PowerRequestSystemRequired)`; the reason is visible in `powercfg /requests`. |
| macOS | IOKit [`IOPMAssertionCreateWithName`](https://developer.apple.com/documentation/iokit) (`kIOPMAssertionTypePreventUserIdleSystemSleep`). |
| Linux | Speaks the D-Bus wire protocol directly over the **system** bus (no `libdbus`) and calls [`org.freedesktop.login1.Manager.Inhibit`](https://www.freedesktop.org/software/systemd/man/latest/org.freedesktop.login1.html) with `what="sleep"`, `mode="block"`. |

On Linux this is the same mechanism as `systemd-inhibit --what=sleep`. It
prevents automatic suspend/hibernation while **leaving the screensaver and
display-blanking untouched** — the right behaviour for a background task such as
a network transfer. logind returns a file descriptor and holds the lock until it
is closed, so the library keeps that descriptor open while active. The interface
is served by systemd-logind, or by the compatible `elogind` on non-systemd
distributions; if neither is reachable, `inhibit()` returns `false`.

You can see an active inhibitor with `systemd-inhibit --list` (look for the
`sleep` / `block` row belonging to your process).

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
./build/examples/woke_example 15   # stay awake for 15 seconds
```

## License

MIT — see [LICENSE](LICENSE).
