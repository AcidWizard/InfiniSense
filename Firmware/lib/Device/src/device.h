#pragma once
#include <stdint.h>

// device.h: chip identity and per-boot entropy. Owns nrf.h for both.

namespace device {

// Low 16 bits of the factory device id, read and cached on first call.
uint16_t Id();

// A fresh 16-bit value for this boot, used to seed the mesh sequence counter so
// a rebooted node does not reuse sequence numbers (receivers dedup on
// {source, sequence}).
uint16_t RandomSeed();

}  // namespace device
