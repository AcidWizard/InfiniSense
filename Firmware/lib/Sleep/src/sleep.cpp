#include "sleep.h"

// sleep.cpp: park the CPU between events.
//
// Two implementations, because the two supported cores differ:
//
//   SLEEP_TICKLESS (Seeed/Adafruit core, XIAO): the loop task blocks and the
//   FreeRTOS tickless idle sleeps the CPU, so the 1 kHz RTOS tick does not keep
//   restarting HFCLK. The GPIOTE input edge notifies the task and the heartbeat
//   is the notification timeout.
//
//   otherwise (the DK core has no FreeRTOS): RTC2 compare for the heartbeat and
//   a GPIOTE interrupt for the input, with the CPU parked in raw WFI.

#if defined(SLEEP_TICKLESS)

#include <Arduino.h>
#include <FreeRTOS.h>
#include <task.h>

#include "log.h"

namespace sleep {
namespace {

// Heartbeat period in RTOS ticks; also the notification timeout in UntilEvent.
TickType_t heartbeat_ticks = 0;
TaskHandle_t loop_task = nullptr;

// GPIOTE priority is 3, which is at or below the FreeRTOS syscall ceiling, so
// the FromISR API is safe here.
void PinIsr() {
  BaseType_t higher_priority = pdFALSE;
  vTaskNotifyGiveFromISR(loop_task, &higher_priority);
  portYIELD_FROM_ISR(higher_priority);
}

}  // namespace

void Begin(uint32_t pin, uint32_t heartbeat_seconds) {
  heartbeat_ticks = pdMS_TO_TICKS(heartbeat_seconds * 1000u);
  loop_task = xTaskGetCurrentTaskHandle();
  attachInterrupt(digitalPinToInterrupt(pin), PinIsr, CHANGE);
  LOG_INFO("sleep begin pin=%u level=%d heartbeat=%u",
           static_cast<unsigned>(pin), digitalRead(pin),
           static_cast<unsigned>(heartbeat_seconds));
}

Wake UntilEvent() {
  // Block until the input edge notifies us or the heartbeat elapses. Tickless
  // idle parks the CPU (and stops HFCLK) for the whole period.
  const uint32_t notified = ulTaskNotifyTake(pdTRUE, heartbeat_ticks);
  return (notified == 0) ? Wake::kRtc : Wake::kPin;
}

}  // namespace sleep

#else  // RTC2 + WFI (nrf52840_dk, no RTOS)

#include <Arduino.h>
#include <nrf.h>

#include "log.h"

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
  LOG_INFO("sleep begin pin=%u level=%d heartbeat=%u",
           static_cast<unsigned>(pin), digitalRead(pin),
           static_cast<unsigned>(heartbeat_seconds));

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

#endif
