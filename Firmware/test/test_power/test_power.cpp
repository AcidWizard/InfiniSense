#include <nrf.h>  // stub registers
#include <stdint.h>
#include <unity.h>

#include "power.h"

void setUp(void) {
  NRF_POWER->GPREGRET = 0;
  NRF_POWER->GPREGRET2 = 0;
  NRF_POWER->SYSTEMOFF = 0;
  NRF_WDT->CONFIG = 0;
  NRF_WDT->CRV = 0;
  NRF_WDT->RREN = 0;
  NRF_WDT->TASKS_START = 0;
  for (unsigned i = 0; i < 8; i++) NRF_WDT->RR[i] = 0;
}

void tearDown(void) {}

static void test_retained_roundtrip(void) {
  TEST_ASSERT_FALSE(power::HasRetained());
  power::SetRetained(7);
  TEST_ASSERT_TRUE(power::HasRetained());
  TEST_ASSERT_EQUAL_UINT8(7, power::GetRetained());
}

static void test_retained_version_invalidated(void) {
  // kRetainMagic 0x40 with version 1 gives tag 0x41; anything else is stale.
  NRF_POWER->GPREGRET = 0x40;
  TEST_ASSERT_FALSE(power::HasRetained());
  NRF_POWER->GPREGRET = 0x41;
  TEST_ASSERT_TRUE(power::HasRetained());
  NRF_POWER->GPREGRET = 0x42;
  TEST_ASSERT_FALSE(power::HasRetained());
}

static void test_reset_clears_retained(void) {
  power::SetRetained(1);
  power::Reset();
  TEST_ASSERT_FALSE(power::HasRetained());
  TEST_ASSERT_EQUAL_UINT8(0, power::GetRetained());
}

static void test_watchdog_pauses_in_sleep(void) {
  power::StartWatchdog(10);
  TEST_ASSERT_EQUAL_UINT32(10u * 32768u, NRF_WDT->CRV);
  // SLEEP_Pause keeps a long System ON sleep from tripping the watchdog.
  TEST_ASSERT_EQUAL_UINT32(WDT_CONFIG_SLEEP_Pause,
                           NRF_WDT->CONFIG & (1u << WDT_CONFIG_SLEEP_Pos));
  TEST_ASSERT_EQUAL_UINT32(WDT_RREN_RR0_Msk, NRF_WDT->RREN);
  TEST_ASSERT_EQUAL_UINT32(1u, NRF_WDT->TASKS_START);
}

static void test_watchdog_kick(void) {
  power::KickWatchdog();
  TEST_ASSERT_EQUAL_UINT32(WDT_RR_RR_Reload, NRF_WDT->RR[0]);
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_retained_roundtrip);
  RUN_TEST(test_retained_version_invalidated);
  RUN_TEST(test_reset_clears_retained);
  RUN_TEST(test_watchdog_pauses_in_sleep);
  RUN_TEST(test_watchdog_kick);
  return UNITY_END();
}
