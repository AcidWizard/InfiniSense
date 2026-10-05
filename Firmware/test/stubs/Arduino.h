#pragma once
#include <stddef.h>
#include <stdint.h>

// Host stand-in for the Arduino core, used only by the native test build.
// Tests drive the GPIO level and clock through these extern globals.

#define LOW 0
#define HIGH 1
#define INPUT 0
#define OUTPUT 1
#define INPUT_PULLUP 2
#define INPUT_PULLDOWN 3

extern int StubDigitalLevel;
extern uint32_t StubMillis;

inline uint32_t millis() { return StubMillis; }
inline void pinMode(uint32_t, uint32_t) {}
inline int digitalRead(uint32_t) { return StubDigitalLevel; }
inline void digitalWrite(uint32_t, int) {}
