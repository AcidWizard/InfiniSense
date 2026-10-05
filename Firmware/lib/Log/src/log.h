#pragma once

// log.h: debug-build-only printf-style logging. Everything is a no-op unless
// LOG_ENABLED, so release builds cost nothing on the sensor's energy budget.

#if defined(__PLATFORMIO_BUILD_DEBUG__)
#define LOG_ENABLED 1
#else
#define LOG_ENABLED 0
#endif

// Named "logging" because "log" would collide with the C math function.
namespace logging {

void Begin();
void Info(const char* format, ...);
void Error(const char* format, ...);

}  // namespace logging
