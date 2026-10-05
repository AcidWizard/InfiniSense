#pragma once
#include <stdint.h>

// sleep.h: System ON (WFI) sleep for the sensor. The internal RTC (RTC2) wakes
// it periodically for a heartbeat and a GPIOTE interrupt wakes it on an input
// edge; the Arduino core's RTC1 (millis) is left untouched.

namespace sleep {

enum class Wake { kPin, kRtc };

// Configure the input-edge wake on pin and the RTC heartbeat period.
void Begin(uint32_t pin, uint32_t heartbeat_seconds);

// Park the CPU in WFI until the input pin toggles or the heartbeat elapses, and
// return which one. Spurious wakes (e.g. the core's millis overflow) are
// handled internally.
Wake UntilEvent();

}  // namespace sleep
