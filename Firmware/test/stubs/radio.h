#pragma once
#include <stddef.h>
#include <stdint.h>

// Stub radio for host tests. mesh::Handle() calls radio::Send(); the stub
// records the frame so tests can assert on what would have been transmitted.

namespace radio {

constexpr size_t kStubMax = 255;

struct StubCapture {
  bool sent;
  size_t length;
  uint8_t data[kStubMax];
};

extern StubCapture stub_last;

inline bool Begin() { return true; }
inline void Reset() {}
inline void Sleep() {}
inline void Standby() {}
inline void StartReceive() {}
inline bool Available() { return false; }
inline int Recv(uint8_t*, size_t) { return -1; }

inline bool Send(const uint8_t* data, size_t length) {
  stub_last.sent = true;
  stub_last.length = length;
  for (size_t i = 0; i < length && i < kStubMax; i++) {
    stub_last.data[i] = data[i];
  }
  return true;
}

}  // namespace radio
