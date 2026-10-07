#include "telemetry.h"

#include <Arduino.h>

#include "config.hpp"
#include "device.h"
#include "radio.h"

// telemetry.cpp: newline-delimited JSON written straight to the serial port.
// Output is streamed piecewise so no large buffer is needed for the payload.

namespace telemetry {
namespace {

uint32_t last_status_ms = 0;

void PrintHex(const uint8_t* data, uint8_t length) {
  static const char kHex[] = "0123456789abcdef";
  for (uint8_t index = 0; index < length; index++) {
    Serial.write(kHex[data[index] >> 4]);
    Serial.write(kHex[data[index] & 0x0f]);
  }
}

}  // namespace

void Begin() {
  Serial.begin(115200);
  // Anchor the status cadence to boot so the first line lands one interval in.
  last_status_ms = millis();
}

void Poll() {
  const uint32_t now = millis();
  if (now - last_status_ms < config::kStatusIntervalMs) return;
  last_status_ms = now;
  Serial.print("{\"status\":\"ok\",\"node\":");
  Serial.print(device::Id());
  Serial.print(",\"radio\":");
  Serial.print(radio::Ready() ? 1 : 0);
  Serial.print(",\"uptime_ms\":");
  Serial.print(now);
  Serial.println("}");
}

void EmitEvent(const mesh::Event& event) {
  Serial.print("{\"src\":");
  Serial.print(event.source);
  Serial.print(",\"seq\":");
  Serial.print(event.sequence);
  Serial.print(",\"type\":");
  Serial.print(static_cast<unsigned>(event.type));
  Serial.print(",\"len\":");
  Serial.print(static_cast<unsigned>(event.length));
  Serial.print(",\"payload\":\"");
  PrintHex(event.payload, event.length);
  Serial.println("\"}");
}

}  // namespace telemetry
