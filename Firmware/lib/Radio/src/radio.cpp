#include "radio.h"

#include <RadioLib.h>

#include "config.hpp"
#include "log.h"

// radio.cpp: all RadioLib specifics live here so the rest of the firmware only
// sees the small namespace radio API.

namespace radio {
namespace {

SX1262 radio_module = new Module(config::kLoRaCs, config::kLoRaDio1,
                                 config::kLoRaReset, config::kLoRaBusy);
bool radio_ready = false;

}  // namespace

bool Begin() {
  if (radio_ready) return true;
  const int status = radio_module.begin(
      config::kLoRaFreqKhz / 1000.0f, config::kLoRaBwKhz, config::kLoRaSf,
      config::kLoRaCr, config::kLoRaSyncWord, config::kLoRaTxDbm,
      config::kLoRaPreamble, config::kLoRaTcxoV, false);
  radio_ready = (status == RADIOLIB_ERR_NONE);
  if (radio_ready) radio_module.startReceive();
  if (!radio_ready) logging::Error("radio init failed: %d", status);
  return radio_ready;
}

void Sleep() {
  if (radio_ready) radio_module.sleep();
}

void StartReceive() {
  if (radio_ready) radio_module.startReceive();
}

bool Available() { return radio_ready && radio_module.available(); }

int Recv(uint8_t* data, size_t max_length) {
  if (!radio_ready) return -1;
  const size_t length = radio_module.getPacketLength();
  // Reject rather than truncate: a shortened frame would parse as corrupt.
  if (length > max_length) {
    logging::Error("radio frame too long: %u", static_cast<unsigned>(length));
    return -1;
  }
  const int status = radio_module.readData(data, length);
  if (status != RADIOLIB_ERR_NONE) {
    logging::Error("radio read failed: %d", status);
    return -1;
  }
  return static_cast<int>(length);
}

// Transmits and leaves the radio in standby; the caller decides whether to
// listen again by calling StartReceive(). This keeps a sensor that transmits
// and sleeps from powering up the receiver. RadioLib's transmit() takes a
// non-const pointer, hence the cast: it only reads the buffer.
bool Send(const uint8_t* data, size_t length) {
  if (!radio_ready) return false;
  // RadioLib would return RADIOLIB_ERR_PACKET_TOO_LONG; fail fast instead.
  if (length > config::kRadioMaxPacketBytes) return false;
  radio_module.standby();
  const int status = radio_module.transmit(const_cast<uint8_t*>(data), length);
  if (status != RADIOLIB_ERR_NONE) {
    logging::Error("radio send failed: %d", status);
    return false;
  }
  return true;
}

}  // namespace radio
