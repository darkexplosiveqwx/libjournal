# libjournal

C library for writing to systemd journald without linking against libsystemd.

Implements the native journal protocol over a Unix datagram socket
(`/run/systemd/journal/socket`). No mmap, no journal file access, no
libsystemd dependency -- just the write path.

## Features

- **Zero libsystemd dependency** -- connect directly to the journal socket
- **musl-compatible** -- works with musl-gcc, no glibc assumptions
- **Embeddable** -- single static library, links only against POSIX Threads
- **Thread-safe** -- atomic fd with mutex-protected init/reconnect
- **Auto-reconnect** -- transparently reconnects on ECONNREFUSED / broken pipe
- **Large message support** -- falls back to memfd for payloads over 128 KiB
- **Text and binary fields** -- auto-detects and encodes binary values per the journal wire format

## Building

```sh
./build.sh                    # build library, demo, and tests (release)
./build.sh build --debug      # debug build
./build.sh build --static     # static build with musl-gcc
./build.sh test               # build and run local tests
./build.sh container          # build via podman/docker (Alpine, fully static)
./build.sh container test     # run integration tests in Fedora/systemd
./build.sh clean              # remove build/ and journal-demo
```

`build` is the default command, so `./build.sh --debug` is also valid. Use
`./build.sh --help` for the complete command reference.

CMake options:

| Option | Default | Description |
|---|---|---|
| `BUILD_DEMO` | ON | Build the demo application |
| `BUILD_TESTS` | ON | Build the protocol tests |

To build manually:

```sh
cmake -S . -B build
cmake --build build
```

## Embedding (in-tree)

Copy `src/lib/` into your project:

```sh
cp -r src/lib/ third_party/libjournal/
```

Then in your `CMakeLists.txt`:

```cmake
add_subdirectory(third_party/libjournal)

add_executable(myapp main.c)
target_link_libraries(myapp PRIVATE journal)
```

The library target, includes, and Threads linkage are handled by the
copied `CMakeLists.txt`.

## Usage

```c
#include "journal.h"
#include <sys/syslog.h>

int main(void)
{
    journal_init();

    journal_print(LOG_INFO, "Hello %s", "world");

    journal_send("MESSAGE=Structured message",
                 "PRIORITY=%i", LOG_WARNING,
                 "USER=test",
                 NULL);

    return 0;
}
```

Compile with `-I<path-to-libjournal>/src/lib` and link against `-ljournal -pthread`.

## API

| Function | Description |
|---|---|
| `journal_init()` | Connect to the journal socket. Safe to call multiple times. |
| `journal_close()` | Close the socket. |
| `journal_get_fd()` | Return the underlying fd (-1 if not connected). |
| `journal_print(priority, fmt, ...)` | Send a MESSAGE field with PRIORITY. |
| `journal_send(fmt, ...)` | Send structured key=value fields (NULL-terminated). |
| `journal_sendv(iov, n)` | Send pre-built iovec fields. |

## Comparison with libsystemd

| | libsystemd (sd-journal) | libjournal |
|---|---|---|
| Dependency | libsystemd.so | none (just the socket) |
| musl support | requires patching | native |
| Read journal | yes | no |
| Write journal | yes | yes |
| mmap | yes | no |
| Binary size | ~1 MB+ | ~20 KB (static) |

## License

BSD-3-Clause. See [LICENSE](LICENSE).
