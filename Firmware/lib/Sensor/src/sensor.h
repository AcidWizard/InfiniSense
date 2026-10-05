#pragma once
#include <stdint.h>

// sensor.h: reads and debounces the digital input.

namespace sensor {

// Configure the input. active_low selects both the internal pull (pull-up for
// active-low, pull-down otherwise) and the logical polarity of Read(), so the
// pin's electrical setup and Read() can never disagree.
void Begin(uint32_t pin, bool active_low);
void Reset();
bool Read();
bool Poll(bool& state_out);

}  // namespace sensor
