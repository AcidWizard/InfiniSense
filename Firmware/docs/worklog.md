# Worklog — XIAO bring-up and power optimisation

Running notes from a debugging session bringing up the XIAO nRF52840 +
Wio-SX1262 sensor/router. Most of the work is committed as
**`456f723 router and sensor working, optimized some power things`**.
Uncommitted at time of writing: `README.md`, `docs/packet.md`,
`lib/Config/src/config.hpp` (the BW = 250 kHz change).

## Symptoms at the start

- Router emitted its liveness heartbeat but received nothing.
- Sensor never appeared on the air.

## Bugs found and fixed

1. **SX1262 never received.** The Wio-SX1262 has a two-input RF switch: DIO2
   drives the TX side and a separate **RX-enable pin (D5)** must be driven by
   the MCU. Added `config::kLoRaRxEn` / `config::kLoRaHasRxEn` and
   `radio_module.setRfSwitchPins(rxEn, RADIOLIB_NC)` in `radio::Begin()`.
2. **`radio::Available()` always false.** It used RadioLib's
   `PhysicalLayer::available()`, which only counts direct-mode bytes and is
   always 0 in LoRa packet mode. Now polls
   `getIrqStatus() & RADIOLIB_SX126X_IRQ_RX_DONE`.
3. **RX was not re-armed after every frame.** `mesh::Handle()` only restarted
   RX on the forwarding path. The re-arm now lives in the router loop
   (`src/main.cpp`); the duplicate in `lib/Mesh/src/mesh.cpp` was removed.
4. **Redundant sleep returned `-705`.** `radio::Sleep()` was issued to an
   already-sleeping chip. Added the `radio_asleep` guard.
5. **Role flow.** Removed the `SENSOR_HELLO` bring-up path and restored the
   sleep -> wake -> report loop. `kHeartbeatSeconds` set to **150** for bench
   use (revert to 3600 for deployment).
6. **Debug-only logging.** Added the XIAO debug environments and the
   `LOG_INFO` / `LOG_ERROR` / `LOG_BEGIN` macros so diagnostics compile out of
   release builds (`lib/Log/src/log.h`, `log.cpp`).

## Power investigation

The Seeed/Adafruit core always initialises TinyUSB (VBUS-gated) and runs
FreeRTOS. The original sleep used raw `__WFI()`, which left the 1 kHz RTOS tick
running and cost ~500 uA. Replaced with the **tickless** path for the XIAO
(`SLEEP_TICKLESS` in `lib/Sleep/src/sleep.cpp`): the loop task blocks on
`ulTaskNotifyTake()` and the GPIOTE ISR notifies it. The DK keeps the RTC2 +
`WFI` implementation.

`radio.cpp` also releases the SPI bus around sleep (`SPI.end()` in
`radio::Sleep()`, `SPI.begin()` in `radio::Send()` / `StartReceive()`), because
the Adafruit SPIM keeps drawing current while merely initialised.

Bench project: **`Firmware/bench/sleep_test/`** (standalone). Measured floors:

| Env | What it does | Current |
|-----|--------------|---------|
| `xiao_off_bare` | System OFF, no wake source | **12.6 uA** |
| `xiao_hb` | tickless heartbeat, no GPIO | **1.1 uA** |
| `xiao_hb_wdt` | `xiao_hb` + 2 s watchdog | **1 uA** (WDT pauses correctly) |
| `xiao_hb_spi` | + `SPI.begin()` only | **48 uA** |
| `xiao_hb_spi_end` | + `SPI.begin()` then `SPI.end()` | **14 uA** |
| `sensor_xiao_noradio` | sensor with `radio::Begin()` skipped | **27 uA** |
| full sensor, radio absent | failed `radio::Begin()` | ~96-109 uA |

Findings:

- **D6 was held high.** With `kSensorActiveLow = false` (`INPUT_PULLDOWN`) an
  externally high D6 is a ~250 uA pull-fight. D6 on the XIAO + Wio stack is the
  Wio GPS-TX line; verify its idle level.
- `SPI.begin()` leaks ~48 uA; `SPI.end()` recovers most of it (~14 uA left).
- The failed `radio::Begin()` path leaves ~82 uA (not the SPI bus, since
  `term()` ends it) — likely HFCLK. Not representative of production, where the
  chip is present.

## Current radio configuration (`lib/Config/src/config.hpp`)

- 868 MHz, **BW 250 kHz**, **SF12**, **CR 4/5**, sync `0x12`, TX **+14 dBm**,
  preamble 8.
- `kHeartbeatSeconds = 150`, `kMeshDefaultHops = 3`, `kMeshNetId = 0x01`.
- Both roles share `config.hpp`, so they stay matched. BW went 125 -> 62.5 ->
  250 kHz, prioritising energy over range.

## Other additions

- **`Software/read_telemetry.py`** — parses the router's newline-delimited JSON
  over a ttyACM port.
- **`Software/setup.sh`** — creates a `.venv`, installs pyserial if missing,
  and runs the reader. `.gitignore` updated for the venv.

## Open items / next steps

1. Clean up temporary hooks: the `SENSOR_SKIP_RADIO` guard in `src/main.cpp`
   and the `sensor_xiao_noradio` env in `platformio.ini`; decide whether to
   keep `bench/sleep_test`.
2. Finish the power measurement **with the SX1262 attached** and record the
   flat baseline between reports (the table above is without the radio).
   Remaining candidates: the ~26 uA sensor bring-up and the ~82 uA failed-init
   path.
3. Robustness: make `radio::Begin()` fail clean when the chip is absent (so a
   bad radio does not silently draw ~80 uA).
4. Deployment values: `kHeartbeatSeconds -> 3600`; consider TX +22 dBm and/or a
   better antenna for range (watch peak current and EIRP limits).
5. Optionally strip the temporary `busy_before` / `busy_after` / `skipped`
   diagnostic lines.

## Commands

```bash
cd Firmware
pio run -e sensor_xiao -t upload        # XIAO sensor (release)
pio run -e router_xiao -t upload        # XIAO router
pio run -e sensor_xiao_debug -t upload -t monitor
pio test -e native
cd bench/sleep_test && pio run -e xiao_hb -t upload
python3 ../Software/read_telemetry.py -p /dev/ttyACM0
```
