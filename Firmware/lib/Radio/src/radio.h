#pragma once
#include <stddef.h>
#include <stdint.h>

// radio.h: thin wrapper around the RadioLib SX1262 transceiver. Owns init,
// TX/RX and sleep. Send() leaves the radio in standby; the caller resumes RX.

namespace radio {

bool Begin();
// True once Begin() has successfully initialized the radio.
bool Ready();
void Sleep();
void StartReceive();
bool Available();
int Recv(uint8_t* data, size_t max_length);
bool Send(const uint8_t* data, size_t length);

}  // namespace radio
