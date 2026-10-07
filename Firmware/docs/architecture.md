# Architecture

This document explains how the firmware is put together: the modules, how they
depend on each other, and the exact code path a device takes from power-on to
sleep (or, for routers, from power-on to steady-state forwarding).

## Module map

```
                         ┌──────────────┐
                         │  src/main.cpp│  entry point, role-specific flow
                         └──────┬───────┘
        ┌────────────┬───────────┼────────────┬───────────┬────────┐
        ▼            ▼           ▼            ▼           ▼        ▼
   ┌────────┐   ┌────────┐  ┌────────┐   ┌────────┐  ┌───────┐┌──────┐
   │ Sensor │   │ Power  │  │  Mesh  │   │ Radio  │  │  Log  ││Sleep │
   └───┬────┘   └────────┘  └───┬────┘   └───┬────┘  └───────┘└──────┘
       │                        │            │
       │                        └─────┬──────┘
       │                              │  Mesh uses Radio to send/receive
       │                         ┌────┴─────┐
       │                         │  Device  │  chip id + RNG (FICR/RNG)
       │                         └──────────┘
       ▼
 lib/Config/src/config.hpp  (shared pins, radio + mesh + sensor constants)
```

| Module | Files | Responsibility |
|--------|-------|----------------|
| `main` | `src/main.cpp` | Ties everything together. Decides what happens based on `config::kNodeRole`. |
| `sensor` | `lib/Sensor/src/*` | Reads and debounces the digital input; pull and polarity come from `Begin()`. |
| `power`  | `lib/Power/src/*`  | Versioned retained state and the hardware watchdog (paused while asleep). |
| `sleep`  | `lib/Sleep/src/*`  | Sensor System ON sleep via `UntilEvent()`: FreeRTOS tickless idle on the XIAO, RTC2 + `WFI` on the DK; GPIOTE input-edge wake on both. |
| `radio`  | `lib/Radio/src/*`  | Thin wrapper around the RadioLib `SX1262` object. Owns init, TX/RX, sleep; `Send()` leaves RX off, the caller resumes it. |
| `mesh`   | `lib/Mesh/src/*`   | Packet construction, validation, dedup, rebroadcast, and an inbound event queue. Uses `radio` for I/O. |
| `device` | `lib/Device/src/*` | Caches the chip id (`FICR->DEVICEID[0]`) and provides a per-boot RNG seed; keeps `nrf.h` out of `config.hpp`. |
| `logging`| `lib/Log/src/*`    | Debug-build-only printf-style logging; compiled to no-ops in release. |
| `telemetry`| `lib/Telemetry/src/*` | Router-only, always-on newline-delimited JSON on serial for each received event. Kept out of sensor builds via `lib_ignore`. |
| `config` | `lib/Config/src/config.hpp` | Pin assignments, radio settings, mesh tuning, role enum and `static_assert`s. |

Notes for newcomers:

- **`config.hpp` is application-level but included by the libraries.** `mesh`,
  `radio`, `sensor` and `device` all pull constants from it. It holds only
  `constexpr` values, the role enum, and the two build-flag macros; `nrf.h` lives
  in `device`/`power`.
- **Modules are effectively singletons** backed by file-static state; you do not
  construct them, you call `namespace::Function(...)`. `sensor` and `power`
  expose a `Reset()` for tests and re-initialization; `radio::Begin()` is
  idempotent and has no reset. There is no other teardown path.
- **Inbound events are queued, not callback-driven.** `mesh::Handle()` validates
  a frame, dedups it, enqueues it with `mesh::PopEvent()` for the application,
  and only then decides whether to forward it. `main.cpp` drains the queue in
  `DrainEvents()`; today it streams each event to serial as JSON, but that is
  where a node would *act* on remote data.
- **Error reporting lives in `radio`.** Radio init, TX and RX failures are
  reported through `logging::Error()` (a no-op in release). `sensor` and `power`
  are direct GPIO/register access with no runtime failure modes to report.
- **The sensor sleeps in System ON, not System OFF.** `sleep::UntilEvent()`
  wakes on an input edge (GPIOTE) or the heartbeat, and the WDT is configured
  to pause while the CPU is asleep, so a long sleep cannot trip it. There are
  two implementations, because the cores differ:
  - **XIAO** (`SLEEP_TICKLESS`, Seeed/Adafruit core): the loop task blocks on
    `ulTaskNotifyTake()` and the FreeRTOS tickless idle parks the CPU, so the
    1 kHz RTOS tick does not keep restarting HFCLK. The GPIOTE edge notifies
    the task; the notification timeout is the heartbeat. This is the difference
    between ~250 µA and ~1 µA of sleep current.
  - **DK** (no RTOS): RTC2 compare for the heartbeat plus a GPIOTE edge, parked
    in raw `WFI` with the core's RTC1/`millis()` left untouched.

## Device lifecycle

### Sensor role (`config::kNodeRole == config::Role::kSensor`)

```
power-on
   │
   ▼
setup()
   │  logging::Begin(); logging::Info(...)   debug builds only
   │  sensor::Begin(config::kPinSensor, config::kSensorActiveLow)
   │  radio::Begin()                   init SX1262
   │  mesh::Begin()                    reset dedup/event queue, random seq seed
   │  power::StartWatchdog(2 s)        paused while asleep
   │  sleep::Begin(config::kPinSensor, config::kHeartbeatSeconds)
   │  ReportState(force=false)         send if changed, then radio::Sleep()
   │
   ▼
loop()  ── repeated forever ──
       reason = sleep::UntilEvent()    input edge (GPIOTE) or heartbeat; the
                                       XIAO blocks in the RTOS tickless idle,
                                       the DK park in WFI on RTC2/GPIOTE
      power::KickWatchdog()
      ReportState(force = (reason == sleep::Wake::kRtc))
      radio::Sleep()
```

The sensor reports **on change**, plus a periodic heartbeat
(`config::kHeartbeatSeconds`, default 2.5 minutes) so a gateway can tell it is alive.
It stays in System ON sleep with the internal RTC running. Retained registers
(`GPREGRET`) remember the last reported value across a power loss, and
`ReportState()` only updates retained once a frame is actually on air.

### Router role (`config::kNodeRole == config::Role::kRouter`)

```
power-on
   │
   ▼
setup()
   │  same begin calls as sensor (routers never source events)
   │
   ▼
router branch
   │  telemetry::Begin()             serial up in release too
   │  mesh::SetForward(true)
   │  radio::StartReceive()
   │
   ▼
loop()  ── repeated forever ──
      power::KickWatchdog()           10 s hardware watchdog
      if radio::Available():
          Recv() → mesh::Handle()
              validate → IsDuplicate/Remember → enqueue
               → decrement hops_left → radio::Send()
          StartReceive()   re-arm RX on every path (Send leaves standby;
                           Handle() may return without forwarding)
      DrainEvents()
      telemetry::Poll()               status line every kStatusIntervalMs
```

Routers never sleep. Every packet they receive is validated, checked against
the dedup cache, queued for the application, and then rebroadcast with one
fewer hop remaining until `hops_left` reaches zero. The payload `type` is
passed through opaquely. See [`packet.md`](packet.md) for the rules.

## Typical message path

```
  sensor (change)   router A         router B         router C       any node
       │               │                │                │              │
       │ hop=3         │                │                │              │
       ├──────────────►│                │                │              │
       │               │ validate+dedup │                │              │
       │               │ forward hop=2  │                │              │
       │               ├───────────────►│                │              │
       │               │                │ forward hop=1  │              │
       │               │                ├───────────────►│              │
       │               │                │                │ forward hop=0│
       │               │                │                ├─────────────►│
       │               │                │                │              │ drop
```

The source's original transmission and each rebroadcast carry the same
`source`/`sequence`, so a node that hears the same event twice drops the
duplicate. With `config::kMeshDefaultHops == 3`, an event is rebroadcast by up
to three routers before it stops propagating.
