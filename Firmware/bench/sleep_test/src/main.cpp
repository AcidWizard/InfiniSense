#include <Arduino.h>

#if defined(SLEEP_TEST_WDT)
#include <nrf.h>
#endif

#if defined(SLEEP_TEST_SPI)
#include <SPI.h>
#endif

// sleep_test: minimal sleep bench for the nRF52840 XIAO.
//
//   -e xiao          System ON heartbeat sleep (FreeRTOS tickless idle), 150 s,
//                    with a D6 edge wake
//   -e xiao_hb       same but heartbeat only, no D6 interrupt (isolates the
//                    core's System ON floor from D6 noise)
//   -e xiao_hb_wdt   xiao_hb plus the sensor's 2 s watchdog (tests WDT pause)
//   -e xiao_off      pure System OFF, wake on the D6 edge
//   -e xiao_off_bare pure System OFF, no wake source (recover with the reset
//                    button; double-tap reset to reflash)
//
// No radio, no mesh, no serial. The `_off` images establish the board's floor;
// `xiao` vs `xiao_hb` separate the core's runtime cost from the wake pin.
//
// Measure on the 3V3 rail (preferably the BAT pads), USB unplugged.

#if !defined(SLEEP_TEST_SYSTEM_OFF)
#include <FreeRTOS.h>
#include <task.h>
#endif

namespace {

// XIAO D6 / config::kPinSensor.
constexpr uint32_t kPinWake = 6;
constexpr bool kActiveLow = false;         // config::kSensorActiveLow
constexpr uint32_t kHeartbeatMs = 150000;  // 2.5 min, matching the sensor
// Spare pin pulsed on each wake so a scope can see the heartbeat cheaply.
constexpr uint32_t kPulsePin = 7;

#if !defined(SLEEP_TEST_SYSTEM_OFF)

#if defined(SLEEP_TEST_WDT)
// Mirror power::StartWatchdog: 2 s, paused in sleep.
void StartWatchdog(uint32_t seconds) {
  NRF_WDT->CONFIG = (WDT_CONFIG_SLEEP_Pause << WDT_CONFIG_SLEEP_Pos) |
                    (WDT_CONFIG_HALT_Pause << WDT_CONFIG_HALT_Pos);
  NRF_WDT->CRV = seconds * 32768;
  NRF_WDT->RREN = WDT_RREN_RR0_Msk;
  NRF_WDT->TASKS_START = 1;
}
void KickWatchdog() { NRF_WDT->RR[0] = WDT_RR_RR_Reload; }
#endif

#if !defined(SLEEP_TEST_NO_PIN)
TaskHandle_t loop_task = nullptr;

void WakeIsr() {
  BaseType_t higher_priority = pdFALSE;
  vTaskNotifyGiveFromISR(loop_task, &higher_priority);
  portYIELD_FROM_ISR(higher_priority);
}
#endif  // !SLEEP_TEST_NO_PIN

#endif  // !SLEEP_TEST_SYSTEM_OFF

}  // namespace

#if defined(SLEEP_TEST_SYSTEM_OFF)

void setup() {
#if defined(SLEEP_TEST_NO_WAKE)
  // No wake source at all: the MCU must sit at its floor until reset. This
  // isolates the power path from the D6 sense configuration.
  NRF_POWER->SYSTEMOFF = 1;
#else
  // Deepest sleep with a D6 edge wake; the MCU resets on the edge.
  systemOff(kPinWake, !kActiveLow);
#endif
}

void loop() {}

#else

void setup() {
  pinMode(kPulsePin, OUTPUT);
  digitalWrite(kPulsePin, LOW);

#if defined(SLEEP_TEST_SPI)
  // Only initialise the SPI bus, as a failed radio::Begin() would.
  SPI.begin();
#if defined(SLEEP_TEST_SPI_END)
  SPI.end();
#endif
#endif

#if defined(SLEEP_TEST_WDT)
  StartWatchdog(2);
#endif

#if !defined(SLEEP_TEST_NO_PIN)
  loop_task = xTaskGetCurrentTaskHandle();
  pinMode(kPinWake, kActiveLow ? INPUT_PULLUP : INPUT_PULLDOWN);
  attachInterrupt(digitalPinToInterrupt(kPinWake), WakeIsr, CHANGE);
#endif
}

void loop() {
  // Block until the input edge notifies us or the heartbeat elapses. With the
  // FreeRTOS tickless idle this parks the CPU for the whole period instead of
  // waking on the 1 kHz RTOS tick.
  const uint32_t notified =
      ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(kHeartbeatMs));
  const bool heartbeat = (notified == 0);
  (void)heartbeat;

#if defined(SLEEP_TEST_WDT)
  KickWatchdog();
#endif

  // Short scope pulse: visible wake without an LED load in the measurement.
  digitalWrite(kPulsePin, HIGH);
  delayMicroseconds(50);
  digitalWrite(kPulsePin, LOW);
}

#endif  // SLEEP_TEST_SYSTEM_OFF
