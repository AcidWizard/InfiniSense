#include "radio.h"

#include <Arduino.h>
#include <RadioLib.h>
#include <SPI.h>

#include "config.hpp"
#include "log.h"

// radio.cpp: all RadioLib specifics live here so the rest of the firmware only
// sees the small namespace radio API.

namespace radio {
namespace {

SX1262 radio_module = new Module(config::kLoRaCs, config::kLoRaDio1,
                                 config::kLoRaReset, config::kLoRaBusy);
bool radio_ready = false;
// True while the SX1262 is in sleep. Sending or receiving wakes it, so this
// keeps Sleep() from re-issuing SET_SLEEP to an already-sleeping chip, which
// just times out on the (high) BUSY line.
bool radio_asleep = false;
// True while the SPI bus is initialised. The Adafruit/Seeed SPIM keeps drawing
// current when it is merely initialised, so we end() it before sleeping and
// begin() it again before the next radio access.
bool spi_active = false;

}  // namespace

bool Begin() {
  if (radio_ready) return true;
  LOG_INFO("radio pins cs=%u dio1=%u rst=%u busy=%u rxen=%u",
           static_cast<unsigned>(config::kLoRaCs),
           static_cast<unsigned>(config::kLoRaDio1),
           static_cast<unsigned>(config::kLoRaReset),
           static_cast<unsigned>(config::kLoRaBusy),
           static_cast<unsigned>(config::kLoRaRxEn));
  const int status = radio_module.begin(
      config::kLoRaFreqKhz / 1000.0f, config::kLoRaBwKhz, config::kLoRaSf,
      config::kLoRaCr, config::kLoRaSyncWord, config::kLoRaTxDbm,
      config::kLoRaPreamble, config::kLoRaTcxoV, false);
  radio_ready = (status == RADIOLIB_ERR_NONE);
  // RadioLib begins SPI on init; the chip-not-found path ends it again.
  spi_active = radio_ready;
  LOG_INFO("radio init status=%d ready=%d freq_khz=%u tcxo=%.2f", status,
           radio_ready ? 1 : 0, static_cast<unsigned>(config::kLoRaFreqKhz),
           static_cast<double>(config::kLoRaTcxoV));
  if (radio_ready) {
    // The Wio-SX1262 routes the antenna switch through DIO2; other modules
    // leave it at the RadioLib default.
    if (config::kLoRaDio2AsRfSwitch) {
      const int dio2 = radio_module.setDio2AsRfSwitch(true);
      LOG_INFO("radio dio2_as_rf_switch status=%d", dio2);
      (void)dio2;  // read only by the debug log
    }
    // DIO2 only drives the TX side of the Wio-SX1262 RF switch. Its separate
    // RX-enable pin must be driven by the MCU or the receiver never connects,
    // which lets the node transmit but hear nothing. txEn stays NC: the TX
    // side is already controlled by DIO2.
    if (config::kLoRaHasRxEn) {
      radio_module.setRfSwitchPins(config::kLoRaRxEn, RADIOLIB_NC);
      LOG_INFO("radio rf_switch rxen=%u txen=NC",
               static_cast<unsigned>(config::kLoRaRxEn));
    }
    const int rx = radio_module.startReceive();
    radio_asleep = false;
    LOG_INFO("radiorx listen status=%d", rx);
    (void)rx;  // read only by the debug log
  }
  if (!radio_ready) {
    LOG_ERROR("radio init failed: %d busy_pin=%d", status,
              digitalRead(config::kLoRaBusy));
  }
  return radio_ready;
}

bool Ready() { return radio_ready; }

void Sleep() {
  if (!radio_ready) return;
  // Already powered down: nothing to do. Re-issuing SET_SLEEP to a sleeping
  // chip times out on BUSY, so skip it.
  if (radio_asleep) {
    LOG_INFO("radio sleep skipped (already asleep)");
    return;
  }
  LOG_INFO("radio sleep busy_before=%d", digitalRead(config::kLoRaBusy));
  const int status = radio_module.sleep();
  LOG_INFO("radio sleep status=%d busy_after=%d", status,
           digitalRead(config::kLoRaBusy));
  if (status == RADIOLIB_ERR_NONE) {
    radio_asleep = true;
    // Release the SPI bus too: the Adafruit SPIM keeps drawing current while
    // merely initialised. Send() re-begins it before the next radio access.
    if (spi_active) {
      SPI.end();
      spi_active = false;
    }
  }
}

void StartReceive() {
  if (radio_ready) {
    if (!spi_active) {
      SPI.begin();
      spi_active = true;
    }
    const int status = radio_module.startReceive();
    radio_asleep = false;
    LOG_INFO("radiorx listen status=%d", status);
    (void)status;  // read only by the debug log
  }
}

// RadioLib's PhysicalLayer::available() only counts direct-mode bytes and is
// always 0 in LoRa packet mode (SX126x does not override it), so poll the
// SX126x RxDone IRQ flag instead. getIrqStatus() is read-only; readData()
// clears the flags once the packet is consumed.
bool Available() {
  if (!radio_ready) return false;
  return (radio_module.getIrqStatus() & RADIOLIB_SX126X_IRQ_RX_DONE) != 0;
}

int Recv(uint8_t* data, size_t max_length) {
  if (!radio_ready) {
    LOG_INFO("radiorx not ready");
    return -1;
  }
  const size_t length = radio_module.getPacketLength();
  LOG_INFO("radiorx packet len=%u", static_cast<unsigned>(length));
  // Reject rather than truncate: a shortened frame would parse as corrupt.
  if (length > max_length) {
    LOG_ERROR("radiorx frame too long: %u", static_cast<unsigned>(length));
    return -1;
  }
  const int status = radio_module.readData(data, length);
  if (status != RADIOLIB_ERR_NONE) {
    LOG_ERROR("radiorx read failed: %d", status);
    return -1;
  }
  LOG_INFO("radiorx ok status=%d len=%u", status,
           static_cast<unsigned>(length));
  return static_cast<int>(length);
}

// Transmits and leaves the radio in standby; the caller decides whether to
// listen again by calling StartReceive(). This keeps a sensor that transmits
// and sleeps from powering up the receiver. RadioLib's transmit() takes a
// non-const pointer, hence the cast: it only reads the buffer.
bool Send(const uint8_t* data, size_t length) {
  if (!radio_ready) {
    LOG_ERROR("radiotx not ready");
    return false;
  }
  // RadioLib would return RADIOLIB_ERR_PACKET_TOO_LONG; fail fast instead.
  if (length > config::kRadioMaxPacketBytes) {
    LOG_ERROR("radiotx too long: %u", static_cast<unsigned>(length));
    return false;
  }
  // Re-begin the SPI bus if Sleep() released it; Standby then wakes the chip if
  // it was asleep.
  if (!spi_active) {
    SPI.begin();
    spi_active = true;
  }
  radio_module.standby();
  radio_asleep = false;
  LOG_INFO("radiotx standby busy=%d", digitalRead(config::kLoRaBusy));
  const int status = radio_module.transmit(const_cast<uint8_t*>(data), length);
  if (status != RADIOLIB_ERR_NONE) {
    LOG_ERROR("radiotx failed status=%d len=%u busy=%d", status,
              static_cast<unsigned>(length), digitalRead(config::kLoRaBusy));
    return false;
  }
  LOG_INFO("radiotx ok status=%d len=%u busy=%d", status,
           static_cast<unsigned>(length), digitalRead(config::kLoRaBusy));
  return true;
}

}  // namespace radio
