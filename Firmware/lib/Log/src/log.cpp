#include "log.h"

// log.cpp: serial output in debug builds, empty stubs otherwise.

#if LOG_ENABLED

#include <Arduino.h>
#include <stdarg.h>
#include <stdio.h>

namespace logging {
namespace {

void Emit(const char* level, const char* format, va_list args) {
  char buffer[128];
  vsnprintf(buffer, sizeof(buffer), format, args);
  Serial.print(level);
  Serial.print(": ");
  Serial.println(buffer);
}

}  // namespace

void Begin() { Serial.begin(115200); }

void Info(const char* format, ...) {
  va_list args;
  va_start(args, format);
  Emit("INFO", format, args);
  va_end(args);
}

void Error(const char* format, ...) {
  va_list args;
  va_start(args, format);
  Emit("ERROR", format, args);
  va_end(args);
}

}  // namespace logging

#else

namespace logging {

void Begin() {}
void Info(const char*, ...) {}
void Error(const char*, ...) {}

}  // namespace logging

#endif
