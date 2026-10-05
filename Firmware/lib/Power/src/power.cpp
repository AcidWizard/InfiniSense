#include "power.h"

#include <nrf.h>

// power.cpp: raw nRF POWER/WDT register access for retained state and the
// watchdog.

namespace power {
namespace {

// GPREGRET holds a tag: a magic nibble plus a format version. Bump
// kRetainVersion whenever the meaning of the GPREGRET2 value changes so
// retained data written by an older firmware is ignored (cold boot).
constexpr uint8_t kRetainMagic = 0x40;
constexpr uint8_t kRetainVersion = 1;
constexpr uint8_t kRetainTag = kRetainMagic | kRetainVersion;

// The watchdog counts on the 32.768 kHz clock.
constexpr uint32_t kWatchdogTicksPerSecond = 32768;

}  // namespace

// Clear the retained tag so the next boot behaves like a cold start.
void Reset() {
  NRF_POWER->GPREGRET = 0;
  NRF_POWER->GPREGRET2 = 0;
}

// Pause the count while the CPU is in sleep (WFI) so a long sleep cannot trip
// it; resume counting while the CPU runs. Pause while halted by a debugger.
void StartWatchdog(uint32_t seconds) {
  NRF_WDT->CONFIG = (WDT_CONFIG_SLEEP_Pause << WDT_CONFIG_SLEEP_Pos) |
                    (WDT_CONFIG_HALT_Pause << WDT_CONFIG_HALT_Pos);
  NRF_WDT->CRV = seconds * kWatchdogTicksPerSecond;
  NRF_WDT->RREN = WDT_RREN_RR0_Msk;
  NRF_WDT->TASKS_START = 1;
}

void KickWatchdog() { NRF_WDT->RR[0] = WDT_RR_RR_Reload; }

bool HasRetained() { return NRF_POWER->GPREGRET == kRetainTag; }

uint8_t GetRetained() { return static_cast<uint8_t>(NRF_POWER->GPREGRET2); }

void SetRetained(uint8_t value) {
  NRF_POWER->GPREGRET = kRetainTag;
  NRF_POWER->GPREGRET2 = value;
}

}  // namespace power
