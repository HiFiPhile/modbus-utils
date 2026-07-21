# Changelog

This project follows semantic versioning. Notable user-visible changes are
recorded here.

## 0.2.0 - 2026-07-21

### Added

- One-command builds for the bundled libmodbus dependency, with optional system
  libmodbus support through `pkg-config`.
- Unit, CLI, and TCP loopback tests covering all supported Modbus function
  codes.
- Formatting, strict-warning, sanitizer, and GitHub Actions quality gates.
- Version output and install/uninstall targets.

### Fixed

- Single slave addresses are parsed deterministically.
- Discrete-input reads now execute function code 02.
- Millisecond timeouts are normalized into valid seconds and microseconds.
- Server transport defaults are initialized, and TCP `--ip` no longer collides
  with the slave `--addr` option.
- Protocol limits, write values, mapping sizes, ports, and register ranges are
  validated before I/O.
- Allocation, context creation, connection, reply, and setup errors now produce
  consistent failures and cleanup.
