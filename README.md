# modbus-utils

`modbus-utils` provides two small command-line programs based on libmodbus:

- `modbusc` sends Modbus TCP or RTU requests.
- `modbuss` runs a Modbus TCP or RTU test server with an in-memory data map.

The current project version is recorded in [`VERSION`](VERSION). Linux is the
continuously tested platform. Tagged releases also provide statically linked
MinGW64 binaries for 64-bit Windows; Windows runtime tests are not currently
part of CI.

## Build

Prerequisites are a C compiler, POSIX Make, and the normal tools needed by an
Autotools `configure` script. The default build uses the bundled, pinned
libmodbus source:

```sh
git clone https://github.com/HiFiPhile/modbus-utils
cd modbus-utils
make
```

The binaries are written to `build/modbusc` and `build/modbuss`. The build
configures libmodbus automatically; no manual build in the dependency directory
is required.

To use an installed libmodbus instead:

```sh
make USE_SYSTEM_LIBMODBUS=1
```

This mode requires `pkg-config` and a `libmodbus.pc` file. Add `STATIC=1` to ask
`pkg-config` for static dependencies. Add `DEBUG=1` for an unoptimized debug
build.

Cross-compile static 64-bit Windows binaries with a MinGW64 toolchain:

```sh
make mingw64
```

This produces `build/modbusc.exe` and `build/modbuss.exe` using the bundled
libmodbus source.

Install under `/usr/local` with `make install`, or stage a package with, for
example:

```sh
make install DESTDIR=/tmp/modbus-utils-package PREFIX=/usr
```

## Client examples

Read ten holding registers over TCP:

```sh
build/modbusc tcp --ip 127.0.0.1 --port 502 --addr 1 \
    --reg 0 --func 3 --count 10
```

Write three registers:

```sh
build/modbusc tcp --ip 127.0.0.1 --addr 1 --reg 20 --func 16 \
    --write 1 --write 2 --write 3
```

Read input registers over RTU:

```sh
build/modbusc rtu --dev /dev/ttyUSB0 --baud 19200 --parity E \
    --addr 1 --reg 0 --func 4 --count 4
```

Use an inclusive address range such as `--addr 1.10` to scan slave addresses.
RTU baud rates and parities can also be repeated to scan serial settings. Use
`--base-1` when register values in device documentation start at one.

Supported function codes are:

| Code | Operation |
|---:|---|
| `01` | Read coils |
| `02` | Read discrete inputs |
| `03` | Read holding registers |
| `04` | Read input registers |
| `05` | Write single coil |
| `06` | Write single register |
| `15` (`0F`) | Write multiple coils |
| `16` (`10`) | Write multiple registers |

Run `build/modbusc --help`, `build/modbusc tcp --help`, or
`build/modbusc rtu --help` for the complete option list.

## Server examples

Start a TCP test server on an unprivileged port:

```sh
build/modbuss tcp --ip 127.0.0.1 --port 1502 --addr 1
```

Start an RTU server:

```sh
build/modbuss rtu --dev /dev/ttyUSB0 --baud 19200 --parity E --addr 1
```

The server defaults to 100 coils, discrete inputs, holding registers, and input
registers, all initialized to zero. Adjust those sizes with `--co`, `--di`,
`--hr`, and `--ir`.

TCP port 502 may require elevated privileges on some systems. Prefer a port
above 1024 for local testing. Use `--ip 0.0.0.0` only when the server should be
reachable through every IPv4 interface.

## Tests and development checks

```sh
make check      # formatting and strict compiler diagnostics
make test       # unit, CLI, and TCP loopback tests
make sanitize   # test with AddressSanitizer and UndefinedBehaviorSanitizer
make format     # apply the repository clang-format configuration
```

The default tests use only localhost TCP sockets and require no Modbus hardware.
See [`CONTRIBUTING.md`](CONTRIBUTING.md) for the expected contribution workflow
and [`THIRD_PARTY.md`](THIRD_PARTY.md) for bundled dependency provenance.

Pushing a semantic-version tag such as `0.3.0` runs the release workflow. It
publishes Linux and static MinGW64 archives with a `SHA256SUMS` file. The same
workflow can be started manually for an existing tag.

## Exit status

Both programs return zero for successful commands and nonzero for invalid
arguments, setup failures, connection failures, or requests for which no slave
responded successfully. Diagnostics are written to standard error.

## License

The project is distributed under the MIT License; see [`LICENSE.md`](LICENSE.md).
Bundled dependencies retain their own licenses.
