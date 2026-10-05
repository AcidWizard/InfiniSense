#pragma once
#include <stdint.h>

// Minimal nRF register model for host tests. Registers live in function-local
// statics returned by inline functions so every translation unit shares one
// instance (a header `static` would give each TU its own copy, and the test
// could not observe what the module under test wrote).

// --- FICR: factory device id ---
struct NrfFicr {
  uint32_t DEVICEID[2];
};
inline NrfFicr* StubFicr() {
  static NrfFicr ficr = {{0x0000ABCDu, 0}};
  return &ficr;
}
#define NRF_FICR StubFicr()

// --- RNG ---
// EVENTS_VALRDY always reads as set and ignores writes, so RandomSeed() does
// not spin on the host.
struct StubRngEvent {
  operator uint32_t() const { return 1u; }
  void operator=(uint32_t) {}
};
struct NrfRng {
  uint32_t TASKS_START = 0;
  uint32_t TASKS_STOP = 0;
  StubRngEvent EVENTS_VALRDY;
  uint32_t VALUE = 0xA5;
};
inline NrfRng* StubRng() {
  static NrfRng rng;
  return &rng;
}
#define NRF_RNG StubRng()

// --- POWER: retained registers ---
struct NrfPower {
  uint32_t GPREGRET = 0;
  uint32_t GPREGRET2 = 0;
  uint32_t SYSTEMOFF = 0;
};
inline NrfPower* StubPower() {
  static NrfPower power;
  return &power;
}
#define NRF_POWER StubPower()

// --- WDT ---
struct NrfWdt {
  uint32_t CONFIG = 0;
  uint32_t CRV = 0;
  uint32_t RREN = 0;
  uint32_t TASKS_START = 0;
  uint32_t RR[8] = {0};
};
inline NrfWdt* StubWdt() {
  static NrfWdt wdt;
  return &wdt;
}
#define NRF_WDT StubWdt()

#define WDT_CONFIG_SLEEP_Pos (0UL)
#define WDT_CONFIG_HALT_Pos (3UL)
#define WDT_CONFIG_SLEEP_Pause (0UL << WDT_CONFIG_SLEEP_Pos)
#define WDT_CONFIG_SLEEP_Run (1UL << WDT_CONFIG_SLEEP_Pos)
#define WDT_CONFIG_HALT_Pause (0UL << WDT_CONFIG_HALT_Pos)
#define WDT_RREN_RR0_Msk (0x1UL)
#define WDT_RR_RR_Reload (0x6E524635UL)
