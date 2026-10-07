#include <Arduino.h>

#include "config.hpp"
#include "device.h"
#include "log.h"
#include "mesh.h"
#include "power.h"
#include "radio.h"
#include "sensor.h"
#include "sleep.h"
#if NODE_ROLE == 1
#include "telemetry.h"
#endif

// main.cpp: entry point. The only place the sensor/router flow branches.

// Temporary bench hook: skip radio init to isolate its contribution to sleep
// current (see bench/sleep_test). Off in every normal build.
#ifndef SENSOR_SKIP_RADIO
#define SENSOR_SKIP_RADIO 0
#endif

namespace {

// Consume any events the mesh queued. There is no application behavior yet, so
// this only streams them to the serial host as JSON; real telemetry would act
// on Event here too.
void DrainEvents() {
  mesh::Event event;
  while (mesh::PopEvent(event)) {
#if NODE_ROLE == 1
    telemetry::EmitEvent(event);
#endif
  }
}

// Wait for a debounced reading so switch bounce on wake is not reported as a
// state. Bounded by config::kSensorDebounceMs; if the input never settles in
// that window, fall back to the latest raw read rather than the first sample.
bool ReadStable() {
  bool state = sensor::Read();
  const uint32_t start = millis();
  while (millis() - start < config::kSensorDebounceMs) {
    if (sensor::Poll(state)) return state;
  }
  return sensor::Read();
}

// Report the current input state. The sensor only sends when the value differs
// from the last retained one, unless force is set (the periodic heartbeat),
// which sends regardless so a gateway can tell the node is alive. Retained is
// updated only once the frame is actually on air, so a failed send is retried.
void ReportState(bool force) {
  const bool state = ReadStable();
  const uint8_t data = state ? 1u : 0u;

  if (force || !power::HasRetained() || power::GetRetained() != data) {
    uint8_t frame[mesh::kMaxLength];
    const size_t length = mesh::BuildBinaryEvent(
        frame, sizeof(frame), device::Id(), mesh::NextSeq(), &data,
        sizeof(data), config::kMeshDefaultHops);
    if (length > 0 && radio::Send(frame, length)) {
      power::SetRetained(data);
    }
  }
}

}  // namespace

void setup() {
  LOG_BEGIN();
  LOG_INFO("node=%u role=%d", static_cast<unsigned>(device::Id()),
           static_cast<int>(config::kNodeRole));

  sensor::Begin(config::kPinSensor, config::kSensorActiveLow);
#if !SENSOR_SKIP_RADIO
  radio::Begin();
#endif
  mesh::Begin();

  if (config::kNodeRole == config::Role::kRouter) {
    // Routers are pure relays: they never sleep and keep the radio listening.
    // Bring up serial here rather than relying on logging::Begin(), which is a
    // no-op in release, so a flashed router streams received packets.
#if NODE_ROLE == 1
    telemetry::Begin();
#endif
    power::StartWatchdog(config::kWatchdogRouterSeconds);
    mesh::SetForward(true);
    radio::StartReceive();
  } else {
    // The sensor parks in System ON sleep, waking on an input edge or the RTC
    // heartbeat. Report the state once at boot, then power the radio down.
    power::StartWatchdog(config::kWatchdogSensorSeconds);
    sleep::Begin(config::kPinSensor, config::kHeartbeatSeconds);
    ReportState(false);
    radio::Sleep();
  }
}

void loop() {
  if (config::kNodeRole == config::Role::kRouter) {
    power::KickWatchdog();
    if (radio::Available()) {
      uint8_t buffer[mesh::kMaxLength];
      const int length = radio::Recv(buffer, sizeof(buffer));
      if (length > 0) mesh::Handle(buffer, static_cast<size_t>(length));
      // Re-arm unconditionally. Handle() only restarts RX on the forwarding
      // path, so a duplicate, invalid or CRC-failed frame would otherwise stop
      // the receiver from listening again.
      radio::StartReceive();
    }
    DrainEvents();
#if NODE_ROLE == 1
    telemetry::Poll();
#endif
    return;
  }

  // Sensor: sleep until the input toggles or the heartbeat fires, then report.
  const sleep::Wake reason = sleep::UntilEvent();
  LOG_INFO("sensor wake reason=%d", static_cast<int>(reason));
  power::KickWatchdog();
  ReportState(reason == sleep::Wake::kRtc);
  radio::Sleep();
}
