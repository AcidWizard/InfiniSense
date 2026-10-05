#pragma once
#include <stddef.h>
#include <stdint.h>

// packet.h: the over-the-air wire layout. This is a wire contract -- keep it in
// sync with docs/packet.md and bump kVersion for incompatible changes. The
// header is packed and serialized little-endian (native on the nRF52). The
// payload that follows is variable length, so frames are built into a caller
// buffer with mesh::BuildBinaryEvent() rather than a fixed struct.

// Multi-byte fields are copied natively; the target must be little-endian
// (true for the nRF52 and the host test build).
static_assert(__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__,
              "mesh wire format assumes a little-endian target");

namespace mesh {

constexpr uint8_t kVersion = 0x01;

enum class MeshType : uint8_t { kBinaryEvent = 1, kMeshBeacon = 2 };

struct __attribute__((packed)) MeshHeader {
  uint8_t version;     // wire format version, must match kVersion
  uint8_t network_id;  // logical network id, must match config::kMeshNetId
  uint8_t type;        // MeshType: kBinaryEvent or kMeshBeacon
  uint8_t hops_left;   // remaining re-broadcasts before the packet is dropped
  uint16_t source;     // originating node id (low bits of FICR DEVICEID)
  uint16_t sequence;   // per-source counter, used to drop duplicate packets
  uint8_t length;      // payload length in bytes, at most kMaxPayload
};

// Lock the on-air header layout: inserting/reordering a field must be a
// conscious, version-bumped change, not an accident of packing.
static_assert(sizeof(MeshHeader) == 9, "MeshHeader wire size changed");
static_assert(offsetof(MeshHeader, version) == 0, "MeshHeader.version moved");
static_assert(offsetof(MeshHeader, network_id) == 1,
              "MeshHeader.network_id moved");
static_assert(offsetof(MeshHeader, type) == 2, "MeshHeader.type moved");
static_assert(offsetof(MeshHeader, hops_left) == 3,
              "MeshHeader.hops_left moved");
static_assert(offsetof(MeshHeader, source) == 4, "MeshHeader.source moved");
static_assert(offsetof(MeshHeader, sequence) == 6, "MeshHeader.sequence moved");
static_assert(offsetof(MeshHeader, length) == 8, "MeshHeader.length moved");

// Largest frame that can actually go on air. The SX1262 (via RadioLib) caps a
// LoRa frame at 255 bytes, so the whole mesh frame -- header plus payload -- is
// limited to that even though the 8-bit length field alone would permit more.
// Keep in sync with config::kRadioMaxPacketBytes.
constexpr size_t kMaxLength = 255;

// Largest payload that still fits once the header is accounted for.
constexpr size_t kMaxPayload = kMaxLength - sizeof(MeshHeader);

static_assert(kMaxPayload <= UINT8_MAX,
              "payload length must fit the 8-bit length header field");

}  // namespace mesh
