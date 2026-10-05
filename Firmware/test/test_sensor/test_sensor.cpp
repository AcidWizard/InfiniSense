#include <Arduino.h>
#include <stdint.h>
#include <unity.h>

#include "config.hpp"
#include "sensor.h"

int StubDigitalLevel = HIGH;
uint32_t StubMillis = 0;

void setUp(void) {
  StubDigitalLevel = HIGH;
  StubMillis = 0;
  sensor::Begin(config::kPinSensor, config::kSensorActiveLow);
}

void tearDown(void) {}

static void test_begin_honours_active_low(void) {
  StubDigitalLevel = LOW;
  sensor::Begin(config::kPinSensor, true);
  TEST_ASSERT_TRUE(sensor::Read());

  StubDigitalLevel = HIGH;
  sensor::Begin(config::kPinSensor, true);
  TEST_ASSERT_FALSE(sensor::Read());
}

static void test_read_applies_polarity(void) {
  StubDigitalLevel = HIGH;
  const bool high = sensor::Read();
  StubDigitalLevel = LOW;
  const bool low = sensor::Read();

  if (config::kSensorActiveLow) {
    TEST_ASSERT_TRUE(low);
    TEST_ASSERT_FALSE(high);
  } else {
    TEST_ASSERT_TRUE(high);
    TEST_ASSERT_FALSE(low);
  }
}

static void test_poll_debounces_change(void) {
  bool state = true;

  StubDigitalLevel = LOW;
  TEST_ASSERT_FALSE(sensor::Poll(state));

  StubMillis = config::kSensorDebounceMs - 1;
  TEST_ASSERT_FALSE(sensor::Poll(state));

  StubMillis = config::kSensorDebounceMs;
  TEST_ASSERT_TRUE(sensor::Poll(state));

  if (config::kSensorActiveLow) {
    TEST_ASSERT_TRUE(state);
  } else {
    TEST_ASSERT_FALSE(state);
  }
}

static void test_poll_ignores_glitch(void) {
  bool state = true;

  StubDigitalLevel = LOW;
  TEST_ASSERT_FALSE(sensor::Poll(state));

  StubMillis = 10;
  StubDigitalLevel = HIGH;
  TEST_ASSERT_FALSE(sensor::Poll(state));

  StubMillis = 10 + config::kSensorDebounceMs - 1;
  TEST_ASSERT_FALSE(sensor::Poll(state));

  StubMillis = 10 + config::kSensorDebounceMs;
  TEST_ASSERT_TRUE(sensor::Poll(state));
  TEST_ASSERT_TRUE(state);
}

static void test_reset_rebaselines_debounce(void) {
  bool state = true;

  StubDigitalLevel = LOW;
  TEST_ASSERT_FALSE(sensor::Poll(state));

  StubMillis = config::kSensorDebounceMs;
  sensor::Reset();
  TEST_ASSERT_FALSE(sensor::Poll(state));

  StubMillis += config::kSensorDebounceMs;
  TEST_ASSERT_TRUE(sensor::Poll(state));
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_begin_honours_active_low);
  RUN_TEST(test_read_applies_polarity);
  RUN_TEST(test_poll_debounces_change);
  RUN_TEST(test_poll_ignores_glitch);
  RUN_TEST(test_reset_rebaselines_debounce);
  return UNITY_END();
}
