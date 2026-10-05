# Contributing

Thanks for helping with InfiniSense firmware. This file covers the day-to-day
workflow. For what the code *does*, start with [`README.md`](README.md),
[`docs/architecture.md`](docs/architecture.md) and
[`docs/packet.md`](docs/packet.md).

## Prerequisites

- [PlatformIO Core](https://docs.platformio.org/en/latest/core/installation/index.html)
  (`pio`) or the PlatformIO IDE extension for VS Code. The extension is
  recommended in `.vscode/extensions.json`.
- An nRF52840 board (default target: `nrf52840_dk`) and an SX1262 LoRa module.

PlatformIO downloads the toolchain, framework and libraries automatically on the
first build.

## Build

Run everything from the `Firmware/` directory. Environments are named
`<role>_<buildtype>`.

```bash
pio run                       # default env: sensor_release
pio run -e sensor_release
pio run -e sensor_debug
pio run -e router_release
pio run -e router_debug
```

Always build **both roles** before opening a pull request — a change that
compiles for `sensor` may not compile for `router`, and vice versa.

## Flash and monitor

```bash
pio run -e sensor_release -t upload
pio device monitor            # 115200 baud
```

## Quality checks

> **Status:** formatting, host unit tests and static analysis are wired up.
> The embedded build already uses `-Wextra`, with `-Werror` scoped to our own
> code (`src/` and each `lib/` via its `library.json`).

```bash
pio run                                     # compiles all four environments
clang-format --dry-run --Werror src/*.cpp lib/*/src/* test/*/*.cpp test/stubs/*.h  # formatting check
pio test -e native                          # host unit tests
cppcheck --enable=warning,performance,portability --error-exitcode=1 \
  --std=c++11 --language=c++ --suppress=missingIncludeSystem \
  --suppress=missingInclude --inline-suppr \
  -Ilib/Config/src -Ilib/Device/src -Ilib/Log/src -Ilib/Mesh/src \
  -Ilib/Power/src -Ilib/Radio/src -Ilib/Sensor/src -Ilib/Sleep/src \
  -Itest/stubs src lib test                 # static analysis
```

`pio test -e native` compiles the module logic against the stubs in
`test/stubs/` and runs the Unity suites under `test/`. It needs a host C++
compiler (`g++`); install it from your package manager (e.g.
`sudo apt-get install g++`). The embedded environments are unaffected.

`clang-format` is required for the formatting check; it is available on PyPI
(`pip install clang-format`) or via your OS package manager.

## Conventions

- **Language/std:** C++11 (`gnu++11`), Arduino framework, no exceptions or RTTI
  assumptions.
- **Formatting:** Google C++ style, pinned in [`.clang-format`](.clang-format)
  and [`.editorconfig`](.editorconfig). Run `clang-format -i` on changed files
  before committing.
- **Headers:** `#pragma once`, namespace-per-module (e.g. `namespace mesh`), no
  definitions in headers except small `inline` helpers.
- **Config:** put shared constants in `lib/Config/src/config.hpp`, the single
  source of truth for fixed values. They are `constexpr` in `namespace config`;
  `platformio.ini` only overrides the per-build `NODE_ROLE`/`MESH_FORWARD`
  macros.
- **Naming:** Google C++ style — `snake_case` variables and namespaces,
  `PascalCase` functions and types, `kCamelCase` constants, `ALL_CAPS` macros.
- **Wire format changes:** update [`docs/packet.md`](docs/packet.md) in the same
  change and bump `mesh::kVersion` for incompatible changes.
- **Comments:** explain intent for non-obvious logic (power, retained state,
  dedup, wake). Avoid restating the obvious.
- **Keep `main.cpp` thin:** role flow belongs there; logic belongs in the
  `lib/` modules.

## Adding or changing a module

Modules live in `lib/<Name>/src/<name>.{h,cpp}` and expose a namespace API
(lowercase, e.g. `namespace mesh`). Shared constants live in the `lib/Config`
component; a new module can depend on it the same way `mesh`, `radio` and
`sensor` do.

## Pull requests

- Keep changes focused; describe *what* and *why*.
- Build both roles (see above) and state that you did.
- Update docs when behavior, pins, protocol or environments change.
- Do not commit generated artifacts: `.pio/` and `.vscode/` are gitignored.
