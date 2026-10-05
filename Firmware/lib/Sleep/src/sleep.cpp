#include "sleep.h"

#include <Arduino.h>
#include <nrf.h>

// sleep.cpp: RTC2 compare for the heartbeat and a GPIOTE interrupt for the
// input, with the CPU parked in WFI between events.

namespace sleep {
namespace {

// LFCLK is 32768 Hz; PRESCALER 4095 gives an 8 Hz counter.
constexpr uint32_t kRtcPrescaler = 4095;
constexpr uint32_t kRtcTicksPerSecond = 8;

volatile bool rtc_fired = false;
volatile bool pin_fired = false;

void PinIsr() { pin_fired = true; }

}  // namespace

// extern "C" so it overrides the weak RTC2 vector-table entry.
extern "C" void RTC2_IRQHandler(void) {
  if (NRF_RTC2->EVENTS_COMPARE[0]) {
    NRF_RTC2->EVENTS_COMPARE[0] = 0;
  }
  NRF_RTC2->TASKS_CLEAR = 1;
  rtc_fired = true;
}

void Begin(uint32_t pin, uint32_t heartbeat_seconds) {
  attachInterrupt(digitalPinToInterrupt(pin), PinIsr, CHANGE);

  NRF_RTC2->TASKS_STOP = 1;
  NRF_RTC2->PRESCALER = kRtcPrescaler;
  NRF_RTC2->CC[0] = heartbeat_seconds * kRtcTicksPerSecond;
  NRF_RTC2->INTENSET = RTC_INTENSET_COMPARE0_Msk;
  NRF_RTC2->TASKS_CLEAR = 1;
  NVIC_ClearPendingIRQ(RTC2_IRQn);
  NVIC_EnableIRQ(RTC2_IRQn);
  NRF_RTC2->TASKS_START = 1;
}

Wake UntilEvent() {
  for (;;) {
    __disable_irq();
    if (pin_fired || rtc_fired) {
      const Wake reason = pin_fired ? Wake::kPin : Wake::kRtc;
      if (reason == Wake::kPin) {
        pin_fired = false;
      } else {
        rtc_fired = false;
      }
      __enable_irq();
      return reason;
    }
    // WFI wakes on a pending NVIC interrupt even with PRIMASK set, so no event
    // can be lost between the check above and sleeping.
    __WFI();
    __enable_irq();
  }
}

}  // namespace sleep
