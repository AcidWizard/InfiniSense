#include "device.h"

#include <nrf.h>

// device.cpp: FICR/RNG access. FICR is constant hardware data, so read it once
// and keep nrf.h out of config.hpp. Only the low 16 bits are used: collisions
// between chips are possible for a small deployment, but the wire format only
// has room for a uint16_t.

namespace device {
namespace {

// Bound the RNG wait so a hardware fault cannot hang boot.
constexpr uint32_t kRngWaitGuard = 100000;

}  // namespace

uint16_t Id() {
  static const uint16_t cached = static_cast<uint16_t>(NRF_FICR->DEVICEID[0]);
  return cached;
}

uint16_t RandomSeed() {
  NRF_RNG->TASKS_START = 1;
  uint16_t seed = 0;
  for (int byte = 0; byte < 2; byte++) {
    for (volatile uint32_t guard = 0; NRF_RNG->EVENTS_VALRDY == 0; guard++) {
      if (guard > kRngWaitGuard) break;
    }
    seed = static_cast<uint16_t>((seed << 8) | NRF_RNG->VALUE);
    NRF_RNG->EVENTS_VALRDY = 0;
  }
  NRF_RNG->TASKS_STOP = 1;
  return seed;
}

}  // namespace device
