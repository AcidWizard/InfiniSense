#include "sensor.h"

#include <Arduino.h>

#include "config.hpp"

// sensor.cpp: GPIO input with polarity and debounce.

namespace sensor {
namespace {

uint32_t sensor_pin = config::kPinSensor;
bool active_low = config::kSensorActiveLow;
bool last_state = false;
uint32_t last_change = 0;

}  // namespace

void Begin(uint32_t pin, bool active_low_config) {
  sensor_pin = pin;
  active_low = active_low_config;
  pinMode(sensor_pin, active_low ? INPUT_PULLUP : INPUT_PULLDOWN);
  last_state = Read();
  last_change = millis();
}

// Re-baseline the debounce state to the current input, discarding any pending
// change. Intended for tests and re-initialization.
void Reset() {
  last_state = Read();
  last_change = millis();
}

// Raw electrical level, with logical polarity applied using the active_low
// setting passed to Begin().
bool Read() {
  const bool raw = digitalRead(sensor_pin) != LOW;
  return active_low ? !raw : raw;
}

// Debounce helper: reports a stable state in state_out once it has held for
// config::kSensorDebounceMs. Used by main to ignore switch bounce on wake.
bool Poll(bool& state_out) {
  const bool now = Read();
  if (now != last_state) {
    last_state = now;
    last_change = millis();
    return false;
  }
  if ((millis() - last_change) >= config::kSensorDebounceMs) {
    state_out = now;
    return true;
  }
  return false;
}

}  // namespace sensor
