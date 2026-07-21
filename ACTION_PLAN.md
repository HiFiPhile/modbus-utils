# Repository improvement action plan

This roadmap prioritizes correctness and reproducibility before new features.
Each phase should be delivered as a small, reviewable change, and correctness
fixes should include regression coverage in the same change set.

Implementation status: phases 1 through 5 were completed in the initial
repository-improvement pass. Optional RTU pseudo-terminal tests remain a future
enhancement because the default suite is intentionally hardware-free.

## 1. Clean, reproducible build and dependency baseline

- Make a clean checkout build both programs with one command.
- Use standard Make variables (`CPPFLAGS`, `CFLAGS`, `LDFLAGS`, and `LDLIBS`),
  complete source/header dependencies, and portable recursive Make invocations.
- Keep the bundled libmodbus version pinned and document its provenance; allow a
  system libmodbus selected through `pkg-config` as an explicit option.
- Ignore all generated build and Autotools files so a build leaves the working
  tree clean.

Completion criteria: `make` succeeds from a clean checkout, incremental builds
work, and `git status --short` remains empty after building.

## 2. Client/server correctness and defensive validation

- Fix single-address parsing and implement discrete-input reads.
- Initialize all documented server defaults and remove ambiguous option names.
- Convert millisecond timeouts correctly and check every libmodbus setup call.
- Validate slave addresses, ports, register ranges, counts, mapping sizes, and
  write values against protocol and storage limits before performing I/O.
- Check allocations/context creation, use consistent exit statuses, and
  consolidate cleanup paths.
- Replace unchecked numeric parsing and unsafe formatting with bounded,
  type-correct operations.

Completion criteria: valid operations have stable behavior, while invalid input
fails before I/O with a useful diagnostic and a nonzero status.

## 3. Project-owned tests

- Extract parsing and validation into functions that can be unit tested.
- Add CLI tests for help, defaults, invalid inputs, and exit statuses.
- Add TCP loopback coverage for Modbus functions 01, 02, 03, 04, 05, 06, 0F,
  and 10, including address scanning where practical.
- Add optional pseudo-terminal integration tests for RTU behavior without
  requiring physical hardware.

Completion criteria: `make test` is deterministic, requires no hardware for its
default suite, and covers every defect fixed in phase 2.

## 4. Automated quality gates and portability

- Add format, strict-warning, and sanitizer targets.
- Check GCC and Clang builds with `-Wall -Wextra -Wpedantic`.
- Run the test suite under AddressSanitizer and UndefinedBehaviorSanitizer.
- Add CI for the repository's hosting platform and a MinGW compile check if
  Windows remains a supported target.

Completion criteria: merges require successful builds, tests, formatting,
warning checks, and sanitizer checks on the supported platforms.

## 5. Maintainability, documentation, and releases

- Separate CLI parsing, transport setup, request execution, and formatting into
  focused modules as those areas are touched.
- Expand the README with prerequisites, reproducible build instructions,
  supported function codes, TCP/RTU examples, defaults, and troubleshooting.
- Add dependency provenance, a support matrix, `--version`, install/uninstall
  targets, and a changelog/release process.

Completion criteria: a new contributor can build, test, and run both programs
using the repository documentation alone.
