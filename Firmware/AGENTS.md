# AGENTS.md

Guidance for AI coding agents working in `Firmware/`. Humans should read
[`CONTRIBUTING.md`](CONTRIBUTING.md) instead.

## What this is

PlatformIO / Arduino firmware for a battery-free LoRa sensor on nRF52840
(SX1262 radio). One source tree builds two roles selected by the numeric
`NODE_ROLE` flag and exposed as `config::Role`: `kSensor` (sleeps in System ON,
wakes on input change or an RTC heartbeat) and `kRouter` (always-on mesh relay).

## Map of the code

| Path | Purpose |
|------|---------|
| `platformio.ini` | Build environments `<role>_<buildtype>`; some config via `build_flags`. |
| `lib/Config/src/config.hpp` | Pins, radio, mesh, sensor constants; `enum class Role` and typed `constexpr` values. |
| `src/main.cpp` | `setup()`/`loop()`; the only place role flow branches. |
| `lib/Radio/` | SX1262 wrapper (RadioLib). |
| `lib/Mesh/` | `packet.h` wire format; `mesh.*` validate/dedup/forward + event queue. |
| `lib/Sensor/` | Digital input read/debounce. |
| `lib/Power/` | Versioned retained state, watchdog (paused while asleep). |
| `lib/Sleep/` | System ON sleep: RTC2 heartbeat + GPIOTE input-edge wake. |
| `lib/Device/` | Cached chip id + RNG boot seed; owns `nrf.h` so Config does not. |
| `lib/Log/` | Debug-build-only logging (namespace `logging`); no-ops in release. |
| `lib/Telemetry/` | Router-only serial output of received mesh events (namespace `telemetry`); newline-delimited JSON. Excluded from sensor envs via `lib_ignore`. |
| `test/` | Host Unity suites + `stubs/` for the `native` env. |
| `docs/` | `architecture.md`, `packet.md`. |
| `stepstodo.md` | Maintainability backlog (do not treat as done). |

## Commands

Run from `Firmware/`. Host unit tests run under the `native` environment.

```bash
pio run -e sensor_release
pio run -e sensor_debug
pio run -e router_release
pio run -e router_debug
pio test -e native
pio test -e native_coverage   # same tests, --coverage for gcovr
pio run -e sensor_release -t upload
pio device monitor
```

After code changes, build **both** release roles (and run `pio test -e native`
if you touched module logic). Do not invent commands that are not listed here
or in `CONTRIBUTING.md`.

## Rules

- Match existing style: Google C++ naming (`snake_case` variables and
  namespaces, `PascalCase` functions and types, `kCamelCase` constants,
  `ALL_CAPS` macros), `#pragma once`, namespace-per-module, C++11.
- Do not add comments that merely restate code; explain intent for power,
  retained state, dedup and wake logic.
- Changing `lib/Mesh/src/packet.h` requires updating `docs/packet.md` and
  bumping `mesh::kVersion` for incompatible changes.
- Do not commit `.pio/` or machine-specific `.vscode/` files.
- Keep `main.cpp` limited to role flow; put logic in `lib/` modules.
- Known issues and planned refactors are listed in `stepstodo.md`; prefer
  following its conventions rather than introducing new patterns.
