#pragma once
#include <stdint.h>

// power.h: nRF power control -- versioned retained state and the watchdog.
// (Sleep/wake is handled by the Sleep module.)

namespace power {

void Reset();

// Start the hardware watchdog for seconds and reload it with KickWatchdog().
// It is paused while the CPU is asleep (WFI), so a long System ON sleep cannot
// trip it; the router kicks it in loop() and the sensor kicks it each wake.
void StartWatchdog(uint32_t seconds);
void KickWatchdog();

// Retained state survives a reset (GPREGRET2, tagged in GPREGRET) so a sensor
// that loses power does not re-report an unchanged value. The tag carries a
// version; bump it to invalidate data written by an older firmware.
bool HasRetained();
uint8_t GetRetained();
void SetRetained(uint8_t value);

}  // namespace power
