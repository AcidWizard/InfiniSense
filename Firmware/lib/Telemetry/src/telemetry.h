#pragma once

#include "mesh.h"

// telemetry.h: always-on, machine-readable serial output of received mesh
// events. Unlike logging (compiled out in release), this is linked into every
// build so an always-on router can stream packets to a host over the serial
// monitor for another program to consume.

namespace telemetry {

void Begin();

// Emit one received event as a single newline-terminated JSON object. The
// payload is hex encoded so arbitrary binary data stays on one line.
void EmitEvent(const mesh::Event& event);

}  // namespace telemetry
