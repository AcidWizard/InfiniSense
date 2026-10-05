#include <Arduino.h>

#include "config.hpp"
#include "device.h"
#include "log.h"
#include "mesh.h"
#include "power.h"
#include "radio.h"
#include "sensor.h"
#include "sleep.h"

// main.cpp: entry point. The only place the sensor/router flow branches.

namespace {

// Consume any events the mesh queued. There is no application behavior yet, so
// this only reports them; real telemetry would act on Event here.
void DrainEvents() {
  mesh::Event event;
  while (mesh::PopEvent(event)) {
    logging::Info("event src=%u seq=%u type=%u len=%u",
                  static_cast<unsigned>(event.source),
                  static_cast<unsigned>(event.sequence),
                  static_cast<unsigned>(event.type),
                  static_cast<unsigned>(event.length));
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
  logging::Begin();
  logging::Info("node=%u role=%d", static_cast<unsigned>(device::Id()),
                static_cast<int>(config::kNodeRole));

  sensor::Begin(config::kPinSensor, config::kSensorActiveLow);
  radio::Begin();
  mesh::Begin();

  if (config::kNodeRole == config::Role::kRouter) {
    // Routers are pure relays: they never sleep and keep the radio listening.
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
    }
    DrainEvents();
    return;
  }

  // Sensor: sleep until the input toggles or the heartbeat fires, then report.
  const sleep::Wake reason = sleep::UntilEvent();
  power::KickWatchdog();
  ReportState(reason == sleep::Wake::kRtc);
  radio::Sleep();
}
