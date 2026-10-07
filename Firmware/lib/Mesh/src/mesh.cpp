#include "mesh.h"

#include <string.h>

#include "config.hpp"
#include "device.h"
#include "radio.h"

// mesh.cpp: frame build/validate/dedup/forward plus the inbound event queue.

// The mesh frame must fit in one SX1262 LoRa frame. If either limit changes,
// this fails to compile rather than producing un-sendable packets at runtime.
static_assert(mesh::kMaxLength <= config::kRadioMaxPacketBytes,
              "mesh frame exceeds the SX1262 packet limit");

namespace mesh {
namespace {

// Fixed-size ring of recently handled {source, sequence} pairs. Small enough to
// scan linearly, large enough to cover the window in which a mesh may deliver
// copies of the same event via different paths.
struct DedupEntry {
  uint16_t source;    // originating node id of a recently handled packet
  uint16_t sequence;  // that node's sequence number for the packet
  bool valid;         // false for an unused slot, so {0, 0} is not a duplicate
};

DedupEntry seen_cache[config::kMeshDedupSize];
uint8_t seen_index = 0;
uint16_t sequence_counter = 0;
bool forward_flag = config::kMeshForwardDefault;

// Ring of events received but not yet consumed by the application. Overwriting
// the oldest entry when full favours the freshest data.
Event event_queue[config::kMeshEventQueueSize];
uint8_t event_head = 0;
uint8_t event_count = 0;

// Write straight into the ring slot and copy only the payload bytes that are
// present, rather than copying a whole mostly-empty Event struct.
void Enqueue(uint16_t source, uint16_t sequence, MeshType type,
             const uint8_t* payload, uint8_t length) {
  Event* slot;
  if (event_count < config::kMeshEventQueueSize) {
    slot =
        &event_queue[(event_head + event_count) % config::kMeshEventQueueSize];
    event_count++;
  } else {
    slot = &event_queue[event_head];
    event_head =
        static_cast<uint8_t>((event_head + 1) % config::kMeshEventQueueSize);
  }
  slot->source = source;
  slot->sequence = sequence;
  slot->type = type;
  slot->length = length;
  if (length > 0) {
    memcpy(slot->payload, payload, length);
  }
}

}  // namespace

void Begin() {
  memset(seen_cache, 0, sizeof(seen_cache));
  seen_index = 0;
  event_head = 0;
  event_count = 0;
  // A fresh random start per boot avoids reusing sequence numbers after a
  // reboot or wake, which other nodes would drop as duplicates.
  sequence_counter = device::RandomSeed();
  forward_flag = config::kMeshForwardDefault;
}

void SetForward(bool enable) { forward_flag = enable; }

bool ForwardEnabled() { return forward_flag; }

uint16_t NextSeq() { return ++sequence_counter; }

bool PopEvent(Event& out) {
  if (event_count == 0) return false;
  out = event_queue[event_head];
  event_head =
      static_cast<uint8_t>((event_head + 1) % config::kMeshEventQueueSize);
  event_count--;
  return true;
}

uint8_t PendingEvents() { return event_count; }

size_t BuildBinaryEvent(uint8_t* out, size_t capacity, uint16_t source,
                        uint16_t sequence, const void* payload,
                        size_t payload_length, uint8_t hops_left) {
  if (out == nullptr) return 0;
  if (payload_length > kMaxPayload) return 0;
  if (payload_length > 0 && payload == nullptr) return 0;

  const size_t total = sizeof(MeshHeader) + payload_length;
  if (total > kMaxLength || total > capacity) return 0;

  MeshHeader header;
  header.version = kVersion;
  header.network_id = config::kMeshNetId;
  header.type = static_cast<uint8_t>(MeshType::kBinaryEvent);
  header.hops_left = hops_left;
  header.source = source;
  header.sequence = sequence;
  header.length = static_cast<uint8_t>(payload_length);

  memcpy(out, &header, sizeof(header));
  if (payload_length > 0) {
    memcpy(out + sizeof(header), payload, payload_length);
  }
  return total;
}

// True if the pair is in the recent-handled ring. No side effect.
bool IsDuplicate(uint16_t source, uint16_t sequence) {
  for (uint8_t index = 0; index < config::kMeshDedupSize; index++) {
    if (seen_cache[index].valid && seen_cache[index].source == source &&
        seen_cache[index].sequence == sequence)
      return true;
  }
  return false;
}

// Record a pair as handled. Handle() calls this only after the frame is fully
// validated, so a malformed frame cannot occupy a cache slot.
void Remember(uint16_t source, uint16_t sequence) {
  seen_cache[seen_index].source = source;
  seen_cache[seen_index].sequence = sequence;
  seen_cache[seen_index].valid = true;
  seen_index = static_cast<uint8_t>((seen_index + 1) % config::kMeshDedupSize);
}

// Validate an inbound frame, drop duplicates, enqueue it for the application,
// and -- if forwarding is enabled -- rebroadcast it with one fewer hop. A valid
// frame is enqueued regardless of the forwarding flag, so consuming and
// forwarding are independent. Returns true if the frame was a new valid event.
bool Handle(const uint8_t* data, size_t length) {
  if (length < sizeof(MeshHeader)) return false;
  MeshHeader header;
  memcpy(&header, data, sizeof(header));

  if (header.version != kVersion) return false;
  if (header.network_id != config::kMeshNetId) return false;
  if (header.hops_left == 0) return false;

  // Validate before marking seen, so a malformed frame cannot poison the
  // dedup cache and suppress a later valid copy.
  const size_t total = sizeof(MeshHeader) + header.length;
  if (total > kMaxLength) return false;
  if (length < total) return false;
  if (IsDuplicate(header.source, header.sequence)) return false;
  Remember(header.source, header.sequence);

  // The type field is passed through opaquely; the application interprets it.
  Enqueue(header.source, header.sequence, static_cast<MeshType>(header.type),
          data + sizeof(MeshHeader), header.length);

  if (!forward_flag) return true;

  // Decrement before relaying so the last copy goes out with hops_left == 0
  // and stops at the next node.
  header.hops_left = static_cast<uint8_t>(header.hops_left - 1);

  uint8_t buffer[kMaxLength];
  memcpy(buffer, data, total);
  memcpy(buffer, &header, sizeof(header));

  radio::Send(buffer, total);
  // Send() leaves the radio in standby; the caller re-arms receive once
  // Handle() returns, covering this forward path and the duplicate/invalid
  // paths alike.
  return true;
}

}  // namespace mesh
