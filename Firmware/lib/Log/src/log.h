#pragma once

// log.h: debug-build-only printf-style logging. Call sites use the LOG_*
// macros; in release they expand to nothing, so no serial output, format
// strings or argument evaluation end up in the energy-constrained image. In
// debug they forward to namespace logging (named "logging" so it does not
// collide with the C math function).

#if defined(__PLATFORMIO_BUILD_DEBUG__)
#define LOG_ENABLED 1
#else
#define LOG_ENABLED 0
#endif

#if LOG_ENABLED

namespace logging {

void Begin();
void Info(const char* format, ...);
void Error(const char* format, ...);

}  // namespace logging

#define LOG_BEGIN() logging::Begin()
#define LOG_INFO(...) logging::Info(__VA_ARGS__)
#define LOG_ERROR(...) logging::Error(__VA_ARGS__)

#else  // release: compile the logging calls (and their arguments) away

#define LOG_BEGIN() ((void)0)
#define LOG_INFO(...) ((void)0)
#define LOG_ERROR(...) ((void)0)

#endif
