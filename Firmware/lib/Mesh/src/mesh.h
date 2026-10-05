#pragma once
#include <stddef.h>
#include <stdint.h>

#include "packet.h"

// mesh.h: packet construction, validation, duplicate suppression, rebroadcast
// and the inbound event queue. Uses Radio for I/O.

namespace mesh {

// A received event handed to the application. The payload is copied out of the
// radio buffer so it survives until the application drains the queue.
struct Event {
  uint16_t source;
  uint16_t sequence;
  MeshType type;
  uint8_t length;
  uint8_t payload[kMaxPayload];
};

void Begin();
void SetForward(bool enable);
bool ForwardEnabled();
uint16_t NextSeq();

// Drain the inbound event queue. Handle() enqueues each new valid event exactly
// once (before deciding whether to forward it), so a node can act on traffic
// even when forwarding is disabled. PopEvent() returns false when empty.
bool PopEvent(Event& out);
uint8_t PendingEvents();

// Serialize a kBinaryEvent frame into out: a MeshHeader followed by
// payload_length bytes copied from payload. The payload may be any length up to
// kMaxPayload, so the caller owns the backing buffer. Returns the total frame
// length in bytes, or 0 if the payload is too large or the frame does not fit
// within capacity.
size_t BuildBinaryEvent(uint8_t* out, size_t capacity, uint16_t source,
                        uint16_t sequence, const void* payload,
                        size_t payload_length, uint8_t hops_left);

// Duplicate suppression is split into a query and a record so the side effect
// is explicit: Handle() validates a frame, checks IsDuplicate(), then calls
// Remember(). Only IsDuplicate() has no side effect.
bool IsDuplicate(uint16_t source, uint16_t sequence);
void Remember(uint16_t source, uint16_t sequence);

bool Handle(const uint8_t* data, size_t length);

}  // namespace mesh
