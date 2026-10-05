# InfiniSense Firmware

Firmware for **InfiniSense**, a battery-free, capacitor-based energy-harvesting
sensor. Each device reports its input state over LoRa — when the input changes,
plus a periodic heartbeat — and sleeps between events. A self-forming mesh lets
dedicated router nodes relay those events across a site.

The firmware targets the **nRF52840** (currently the `nrf52840_dk` board) and is
built with [PlatformIO](https://platformio.org/) using the Arduino framework and
[RadioLib](https://github.com/jgromes/RadioLib) driving an **SX1262** LoRa
transceiver.

> New here? Read this page, then [`docs/architecture.md`](docs/architecture.md)
> for how the modules fit together and [`docs/packet.md`](docs/packet.md) for
> the over-the-air protocol.

## Node roles

A single firmware source builds into one of two roles, selected at compile time
by the numeric `NODE_ROLE` build flag (see
[`lib/Config/src/config.hpp`](lib/Config/src/config.hpp)), which is exposed as
`config::kNodeRole` (`config::Role::kSensor` or `config::Role::kRouter`):

| Role           | `NODE_ROLE` | Behavior |
|----------------|-------------|----------|
| **Sensor**     | `0` (`kSensor`) | Battery-free end device. Reports its input state on change and on a periodic heartbeat, sleeping in **System ON** (`WFI`) with the internal RTC between events. Woken by an input edge or the heartbeat (`config::kHeartbeatSeconds`). |
| **Router**     | `1` (`kRouter`) | Mains/always-powered relay. Stays awake, forwards mesh packets, and extends range between sensors and whoever consumes the data. |

`MESH_FORWARD` sets the compile-time default for whether a node rebroadcasts
traffic (`config::kMeshForwardDefault`). The router role overrides it to `true`
at runtime; sensors leave it `false`. See
"[Role vs. forwarding flags](#role-vs-forwarding-flags)" below.

## Mesh

Sensors are usually out of range of a gateway, so routers rebroadcast events
hop-by-hop. A packet carries a `hops_left` counter, a `network_id`, and a
per-source `sequence` number so nodes can drop packets they have already seen.
The protocol is small and deliberately simple; see
[`docs/packet.md`](docs/packet.md) for the exact byte layout and hop/dedup
rules.

## Hardware / pin map

Defaults live in [`lib/Config/src/config.hpp`](lib/Config/src/config.hpp) and can be
overridden with `build_flags` in `platformio.ini` or per environment.

| Signal        | Default pin | Notes |
|---------------|-------------|-------|
| Sensor input  | `config::kPinSensor` = 2 | Wakes the sleeping sensor on an edge. Polarity set by `config::kSensorActiveLow` (false = active-high, true = active-low). |
| SX1262 CS     | `config::kLoRaCs` = 10 | SPI chip select |
| SX1262 DIO1   | `config::kLoRaDio1` = 9 | Interrupt / `Available()` |
| SX1262 RESET  | `config::kLoRaReset` = 8 | |
| SX1262 BUSY   | `config::kLoRaBusy` = 7 | |

Radio defaults: **868.0 MHz**, bandwidth 125 kHz, spreading factor 9, coding
rate 4/7, sync word `0x12`, TX power 14 dBm, preamble 8, TCXO 1.6 V. These are
shared by every node and must match for nodes to hear each other.

## Building and flashing

Requires PlatformIO Core (`pio`) or the PlatformIO IDE extension. From the
`Firmware/` directory:

```bash
# Build (default environment is sensor_release)
pio run

# Build a specific role / build type
pio run -e sensor_release     # battery-free sensor, optimized
pio run -e sensor_debug       # sensor with debug build type
pio run -e router_release     # always-on router, optimized
pio run -e router_debug       # router with debug build type

# Flash a board and open the serial monitor
pio run -e sensor_release -t upload
pio device monitor            # 115200 baud
```

An environment is named `<role>_<buildtype>`; roles are `sensor` and `router`,
build types are `debug` and `release`.

## Running a second node

The mesh is peer-to-peer, so bring-up is just "flash more boards":

1. Flash at least one board as a **router**: `pio run -e router_release -t upload`.
2. Flash one or more boards as **sensors**: `pio run -e sensor_release -t upload`.
3. Power the router(s) and leave them on. Trigger the sensor input — the sensor
   boots, transmits the new state, and returns to sleep. A router in range
   rebroadcasts the event.
4. All nodes must share the same `config::kMeshNetId` and radio settings to
   form one network.

Because each environment is a separate build, the role is fixed at flash time;
there is no runtime role switching.

## Role vs. forwarding flags

Three related names appear in the code. They are not interchangeable:

- **`NODE_ROLE`** — the numeric build flag choosing the whole wake/sleep vs.
  stay-awake program flow, exposed as `config::kNodeRole`. This is the flag you
  normally set.
- **`MESH_FORWARD`** — compile-time default for the runtime *forwarding* flag
  (`config::kMeshForwardDefault`). Sensors keep it off; the router role turns it
  on in `setup()`.
- **`mesh::SetForward()` / `mesh::ForwardEnabled()`** — the runtime forwarding
  flag itself, initialized from `MESH_FORWARD` and read by `mesh::Handle()`.

## Debugging

- **Serial logging is debug-only.** It is compiled in when
  `__PLATFORMIO_BUILD_DEBUG__` is set (the `<role>_debug` environments) and is a
  no-op in release. Use `pio run -e sensor_debug -t upload` then
  `pio device monitor` (115200 baud).
- **Routers stream received packets as JSON on serial**, in both release and
  debug builds. Each newly received (deduplicated) event is printed as one JSON
  object per line: `{"src":…,"seq":…,"type":…,"len":…,"payload":"…"}` with the
  payload hex encoded. The `Telemetry` library is router-only and is left out of
  the sensor image. See
  [`lib/Telemetry/src/telemetry.h`](lib/Telemetry/src/telemetry.h).
- **A sensor sleeps between events**, so it looks "dead" most of the time. It
  wakes on an input change or the heartbeat (`config::kHeartbeatSeconds`,
  default 1 hour). Change the input to get an immediate report.
- **Retained state** (the last reported level) survives a reset in the
  `GPREGRET` registers and is versioned, so a firmware change that alters its
  meaning is treated as a cold boot. See
  [`lib/Power/src/power.h`](lib/Power/src/power.h).
- **Watchdog:** both roles run a hardware WDT (router 10 s, sensor 2 s). It is
  paused while the CPU is asleep, so a long System ON sleep cannot trip it.

## Repository layout

```
Firmware/
├── platformio.ini          # build environments (roles x build types)
├── src/main.cpp            # entry point: setup/loop and role flow
├── lib/
│   ├── Config/src/         # shared pins, radio, mesh and sensor configuration
│   ├── Device/src/         # cached chip id + RNG seed (keeps nrf.h out of Config)
│   ├── Log/src/            # debug-only logging (no-op in release)
│   ├── Mesh/src/           # packet.h wire format + mesh.* dedup/forward + event queue
│   ├── Radio/src/          # SX1262 wrapper
│   ├── Power/src/          # versioned retained state, watchdog
│   ├── Sensor/src/         # debounced GPIO sensor input
│   ├── Sleep/src/          # System ON sleep: RTC heartbeat + input-edge wake
│   └── Telemetry/src/      # router-only JSON event stream over serial
├── test/                   # host Unity tests + native stubs
└── docs/                   # architecture and protocol notes
```

## License

MIT — see the repository root [`LICENSE`](../LICENSE).
