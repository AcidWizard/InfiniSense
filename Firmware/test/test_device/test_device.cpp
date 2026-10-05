#include <stdint.h>
#include <unity.h>

#include "device.h"

void setUp(void) {}
void tearDown(void) {}

static void test_id_reads_stub_device_id(void) {
  TEST_ASSERT_EQUAL_UINT16(0xABCD, device::Id());
}

static void test_id_is_stable(void) {
  TEST_ASSERT_EQUAL_UINT16(device::Id(), device::Id());
}

static void test_random_seed_reads_two_rng_bytes(void) {
  // The stub RNG always returns VALUE (0xA5), so the seed is 0xA5A5.
  TEST_ASSERT_EQUAL_UINT16(0xA5A5, device::RandomSeed());
  TEST_ASSERT_EQUAL_UINT16(device::RandomSeed(), device::RandomSeed());
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_id_reads_stub_device_id);
  RUN_TEST(test_id_is_stable);
  RUN_TEST(test_random_seed_reads_two_rng_bytes);
  return UNITY_END();
}
