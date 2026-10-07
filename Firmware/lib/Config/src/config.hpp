#pragma once
#include <stddef.h>
#include <stdint.h>

// config.hpp: the single source of truth for board, radio, mesh and sensor
// tuning. Only genuinely per-build selections come from platformio.ini:
//   - NODE_ROLE    (0 = sensor, 1 = router)
//   - MESH_FORWARD (compile-time default for the runtime forwarding flag)
// Those stay macros because they are set with -D; every other value is a typed
// constexpr in namespace config.

#ifndef NODE_ROLE
#define NODE_ROLE 0
#endif

#ifndef MESH_FORWARD
#define MESH_FORWARD 0
#endif

namespace config {

// NODE_ROLE selects the whole program flow (sleep vs. stay awake).
enum class Role : uint8_t { kSensor = 0, kRouter = 1 };
constexpr Role kNodeRole = static_cast<Role>(NODE_ROLE);

// Compile-time default for the runtime mesh-forwarding flag. The router role
// turns forwarding on in setup(); sensors keep it off. Not the same thing as
// kNodeRole -- see mesh::SetForward()/mesh::ForwardEnabled().
constexpr bool kMeshForwardDefault = (MESH_FORWARD != 0);

constexpr uint8_t kMeshNetId = 0x01;

// Pin map and radio front-end, selected by the board's own build macro so one
// config serves both targets. Defaults are the Nordic nRF52840-DK wiring; the
// Seeed XIAO nRF52840 with a Wio-SX1262 for XIAO (SKU 113010003, or the
// nRF52840 kit 102010710) uses the XIAO pinout below.
#if defined(ARDUINO_Seeed_XIAO_nRF52840)
// Wio-SX1262 for XIAO: SPI uses the core default (D8 SCK / D9 MISO / D10 MOSI).
constexpr uint32_t kPinSensor = 6;  // D6; radio-free, the router ignores it
constexpr uint32_t kLoRaCs = 4;     // D4
constexpr uint32_t kLoRaDio1 = 1;   // D1
constexpr uint32_t kLoRaReset = 2;  // D2
constexpr uint32_t kLoRaBusy = 3;   // D3
constexpr float kLoRaTcxoV = 1.8f;
// The Wio-SX1262 has a two-input RF switch: DIO2 drives the TX side and a
// separate RX-enable pin must be asserted for the receiver to pass signal.
constexpr bool kLoRaDio2AsRfSwitch = true;
constexpr uint32_t kLoRaRxEn = 5;  // D5
constexpr bool kLoRaHasRxEn = true;
#else
constexpr uint32_t kPinSensor = 2;
constexpr uint32_t kLoRaCs = 10;
constexpr uint32_t kLoRaDio1 = 9;
constexpr uint32_t kLoRaReset = 8;
constexpr uint32_t kLoRaBusy = 7;
constexpr float kLoRaTcxoV = 1.6f;
// Generic SX1262 modules route both TX and RX through DIO2; no RX-enable pin.
constexpr bool kLoRaDio2AsRfSwitch = false;
constexpr uint32_t kLoRaRxEn = 0;  // unused when kLoRaHasRxEn is false
constexpr bool kLoRaHasRxEn = false;
#endif

// Sensor heartbeat. The sensor sleeps in System ON (WFI) with the internal RTC
// running and wakes on either an input edge or this timer, reporting the
// current value even if it is unchanged so a gateway can tell the node is
// alive.
constexpr uint32_t kHeartbeatSeconds = 150;

constexpr uint32_t kLoRaFreqKhz = 868000;
// 62.5 kHz buys +3 dB of sensitivity (range) over 125 kHz for 2x the airtime.
constexpr float kLoRaBwKhz = 62.5f;
// Long-range, energy-aware preset: SF12 at 62.5 kHz. Narrow bandwidth buys
// sensitivity more cheaply than SF (3 dB vs 2.5 dB per airtime doubling) and
// CR 4/5 keeps the airtime overhead minimal. Raise kLoRaTxDbm toward 22 for
// maximum range at ~4x the TX current.
constexpr uint8_t kLoRaSf = 12;
constexpr uint8_t kLoRaCr = 5;
constexpr uint8_t kLoRaSyncWord = 0x12;
constexpr int8_t kLoRaTxDbm = 14;
constexpr uint16_t kLoRaPreamble = 8;

// Largest LoRa frame the SX1262 can transmit, matching RadioLib's
// RADIOLIB_SX126X_MAX_PACKET_LENGTH. RadioLib rejects anything longer with
// RADIOLIB_ERR_PACKET_TOO_LONG, so the mesh frame cap must stay within this.
constexpr size_t kRadioMaxPacketBytes = 255;

constexpr uint8_t kMeshDefaultHops = 3;
constexpr uint8_t kMeshDedupSize = 16;
constexpr uint8_t kMeshEventQueueSize = 4;
constexpr uint32_t kSensorDebounceMs = 30;
constexpr bool kSensorActiveLow = false;

// Watchdog timeouts. The router is always on, so it can afford a longer window;
// the sensor only needs to cover boot-to-sleep, which is well under two
// seconds.
constexpr uint32_t kWatchdogRouterSeconds = 10;
constexpr uint32_t kWatchdogSensorSeconds = 2;

// Router liveness. The always-on router emits a status line on the serial
// telemetry stream at this interval so a host can tell it is alive even when no
// mesh traffic arrives.
constexpr uint32_t kStatusIntervalMs = 5000;

// Compile-time range checks. A mistuned radio value would otherwise only be
// caught when RadioLib rejects it at runtime; fail the build instead.
static_assert(kLoRaFreqKhz >= 150000 && kLoRaFreqKhz <= 960000,
              "kLoRaFreqKhz out of the SX1262 range");
static_assert(kLoRaBwKhz == 7.8f || kLoRaBwKhz == 10.4f ||
                  kLoRaBwKhz == 15.6f || kLoRaBwKhz == 20.8f ||
                  kLoRaBwKhz == 31.25f || kLoRaBwKhz == 41.67f ||
                  kLoRaBwKhz == 62.5f || kLoRaBwKhz == 125.0f ||
                  kLoRaBwKhz == 250.0f || kLoRaBwKhz == 500.0f,
              "kLoRaBwKhz must be an SX126x supported bandwidth");
static_assert(kLoRaSf >= 5 && kLoRaSf <= 12, "kLoRaSf out of range");
static_assert(kLoRaCr >= 5 && kLoRaCr <= 8, "kLoRaCr must be 5..8");
static_assert(kLoRaTxDbm >= -9 && kLoRaTxDbm <= 22,
              "kLoRaTxDbm out of the SX1262 range");
static_assert(kLoRaPreamble >= 1, "kLoRaPreamble must be at least 1");
static_assert(kMeshDefaultHops >= 1 && kMeshDefaultHops <= 255,
              "kMeshDefaultHops must fit the hops_left byte");
static_assert(kMeshDedupSize >= 1 && kMeshDedupSize <= 255,
              "kMeshDedupSize must fit the uint8_t ring index");
static_assert(kMeshEventQueueSize >= 1 && kMeshEventQueueSize <= 255,
              "kMeshEventQueueSize must fit the uint8_t queue index");
static_assert(kPinSensor <= 47, "kPinSensor out of range");
static_assert(!kLoRaHasRxEn || kLoRaRxEn <= 47, "kLoRaRxEn out of range");
// The RTC heartbeat uses an 8 Hz counter with a 24-bit compare value.
static_assert(kHeartbeatSeconds >= 1 && kHeartbeatSeconds <= 0xFFFFFFu / 8u,
              "kHeartbeatSeconds must fit the RTC compare counter at 8 Hz");
static_assert(kWatchdogRouterSeconds >= 1 && kWatchdogRouterSeconds <= 65535,
              "watchdog timeout must fit the 32-bit CRV in seconds");
static_assert(kWatchdogSensorSeconds >= 1 && kWatchdogSensorSeconds <= 65535,
              "watchdog timeout must fit the 32-bit CRV in seconds");

}  // namespace config
