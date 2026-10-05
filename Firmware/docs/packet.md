# Mesh wire format

Source of truth: [`lib/Mesh/src/packet.h`](../lib/Mesh/src/packet.h) (header
layout and limits) and [`lib/Mesh/src/mesh.cpp`](../lib/Mesh/src/mesh.cpp)
(serialization, validation, dedup, forwarding). **If you change the structures,
update this document.**

All multi-byte fields are transmitted little-endian (the native byte order of
the Cortex-M4 on the nRF52840). The header is `__attribute__((packed))`, so
there is no padding. `packet.h` `static_assert`s that the target is
little-endian rather than assuming it.

## Frame layout

A frame is a fixed 9-byte header immediately followed by a **variable-length
payload** of `length` bytes:

```
┌──────────────────────── mesh::MeshHeader (9 bytes) ──────────────────────┬──────────────────┐
│ version │ network_id │ type │ hops_left │  source  │ sequence │   length    │ Payload (length) │
│  1 B    │    1 B     │ 1 B  │   1 B     │   2 B    │   2 B    │    1 B      │  0..246 bytes    │
└──────────────────────────────────────────────────────────────────────────┴──────────────────┘
```

The mesh layer treats the payload as opaque bytes. It never parses application
data; it only carries it and preserves it while forwarding. Because the payload
is variable length there is no fixed packet struct — frames are built into a
caller-owned byte buffer with `mesh::BuildBinaryEvent()`.

## `mesh::MeshHeader` — 9 bytes

| Offset | Size | Field        | Type     | Description |
|-------:|-----:|--------------|----------|-------------|
| 0      | 1    | `version`    | `uint8_t`  | Wire-format version. Must equal `mesh::kVersion` (`0x01`). Receivers drop mismatches. |
| 1      | 1    | `network_id` | `uint8_t`  | Logical network. Must equal `config::kMeshNetId` (`0x01`). Lets co-located networks ignore each other. |
| 2      | 1    | `type`       | `uint8_t`  | `mesh::MeshType`: `kBinaryEvent` (1) or `kMeshBeacon` (2). Selects how the payload is interpreted. |
| 3      | 1    | `hops_left`  | `uint8_t`  | Remaining rebroadcasts. A frame with `0` is not forwarded. |
| 4      | 2    | `source`     | `uint16_t` (LE) | Originating node id (truncated `FICR->DEVICEID[0]`). |
| 6      | 2    | `sequence`   | `uint16_t` (LE) | Per-source counter used for duplicate detection. |
| 8      | 1    | `length`     | `uint8_t`  | Payload length in bytes, 0..`mesh::kMaxPayload`. |

## Payload

The payload is application-defined and may be any length from 0 to
`mesh::kMaxPayload` (246) bytes. A sensor sends a `kBinaryEvent` on change and
periodically as a heartbeat; the value may repeat, but each heartbeat carries a
new `sequence` so it is not deduplicated.

| `type`                | Payload |
|-----------------------|---------|
| `kBinaryEvent`        | A single byte: bit 0 is the binary sensor input state (0 or 1). The variable-length framing lets other event types (e.g. a temperature reading) be added without a header change. |
| `kMeshBeacon`         | Reserved; carried like any other type (enqueued/forwarded) but not interpreted. |

> Endianness of the payload is **not** guaranteed by the mesh layer. The current
> `kBinaryEvent` payload is a single byte, so byte order does not apply. For any
> multi-byte payload, define the encoding explicitly for that `type`.

## Building a frame

There is no fixed packet struct. Build a frame into a buffer you own:

```cpp
uint8_t frame[mesh::kMaxLength];
uint8_t payload[100] = { /* ... */ };

const size_t length = mesh::BuildBinaryEvent(
    frame, sizeof(frame), device::Id(), mesh::NextSeq(), payload,
    sizeof(payload), config::kMeshDefaultHops);
if (length > 0) {
  radio::Send(frame, length);
}
```

`BuildBinaryEvent()` copies the header then the payload and returns the total
frame length, or `0` if the payload exceeds `mesh::kMaxPayload` or the frame
does not fit `capacity`. Sending a 100-byte payload is therefore fully
supported.

## Maximum frame size

The whole mesh frame (header + payload) is capped at **255 bytes** to match the
maximum LoRa frame the SX1262 can transmit (`config::kRadioMaxPacketBytes`,
which mirrors RadioLib's `RADIOLIB_SX126X_MAX_PACKET_LENGTH`). RadioLib refuses
longer frames with `RADIOLIB_ERR_PACKET_TOO_LONG`, and `radio::Send()` rejects
them before handing them to the radio.

```
mesh::kMaxLength  = 255 bytes                 (whole frame on air)
mesh::kMaxPayload = 255 - 9 = 246 bytes       (payload after the 9-byte header)
```

`mesh::Handle()` rejects a frame whose `sizeof(MeshHeader) + length` exceeds
`mesh::kMaxLength`, and receive buffers in `main.cpp` are sized to
`mesh::kMaxLength`. A `static_assert` in `packet.h` locks the header at 9 bytes
and the payload at or below 255.

## `type` values

| Name                | Value | Meaning |
|---------------------|------:|---------|
| `kBinaryEvent`      | 1     | A binary sensor state report (one byte). The only type currently produced. |
| `kMeshBeacon`       | 2     | Reserved. `mesh::Handle()` passes `type` through opaquely; the application decides what to do with it. |

## Hop semantics

`mesh::BuildBinaryEvent()` writes `hops_left` from its argument; callers pass
`config::kMeshDefaultHops` (3), which is what the sensor uses. On reception the
forwarding path in `mesh::Handle()`:

1. drops the packet if `hops_left == 0`,
2. otherwise decrements `hops_left` and rebroadcasts the frame.

So the original transmission plus up to **3** router rebroadcasts propagate an
event. The counter is decremented *before* sending, meaning the last forwarded
copy goes out with `hops_left == 0` and is dropped by the next node. The payload
is copied through untouched.

## Duplicate suppression

Each node keeps a fixed-size ring cache of the last `config::kMeshDedupSize`
(16) `{source, sequence}` pairs. `mesh::IsDuplicate(source, sequence)` queries
the cache (no side effect) and `mesh::Remember(source, sequence)` records a
pair. Each entry carries a validity flag, so an unused `{0, 0}` slot is never
mistaken for a real packet.

`source`/`sequence` survive forwarding unchanged, so a node that hears the same
event by multiple paths only processes it once. A pair is remembered only after
the frame passes every validity check (see the validation order below).

## Validation order in `mesh::Handle()`

```
length >= sizeof(MeshHeader)?       → else reject
version == mesh::kVersion?          → else reject
network_id == config::kMeshNetId?   → else reject
hops_left != 0?                     → else reject
sizeof(MeshHeader)+length <= MAX?   → else reject
length bytes actually present?      → else reject
IsDuplicate(source, sequence)?      → else reject (duplicate)
Remember(source, sequence)
enqueue the event for the application
forwarding enabled?                 → else stop (consumed, not forwarded)
decrement hops_left, rebroadcast
```

`IsDuplicate()` is only consulted after the frame is fully validated, so a
malformed frame cannot occupy a dedup slot and suppress a later valid copy.

## Adding a new message type

1. Add a value to `enum class mesh::MeshType` in `packet.h`.
2. Define the payload encoding for the new type and document it in the
   [Payload](#payload) section above.
3. Build it with a serializer that sets the matching `header.type` (extend
   `mesh::BuildBinaryEvent()` or add a sibling such as
   `mesh::BuildTemperatureEvent()`).
4. If the receiver must *act* on the message, drain it from the queue in the
   application via `mesh::PopEvent()`. `mesh::Handle()` enqueues every new valid
   event regardless of whether forwarding is enabled; `main.cpp` currently only
   logs what it drains.

## Payload size and energy

Larger payloads are now a caller choice rather than a code change, but they are
not free. Using RadioLib's own `getTimeOnAir()` with the configured settings
(SF9, BW125 kHz, CR 4/7, CRC on, preamble 8):

| Payload | Frame | Time on air (approx.) |
|--------:|------:|----------------------:|
| 1 B (current `kBinaryEvent`) | 10 B  | ~170 ms |
| 100 B   | 109 B | ~800 ms |

For a capacitor-harvesting sensor, budget the energy and duty cycle before
increasing payloads. If you need more than `mesh::kMaxPayload`, it must be split
across multiple frames at the application layer; the header's 8-bit `length`
and the 255-byte LoRa limit are hard ceilings.

> **Regulatory:** the 868 MHz band has duty-cycle limits (e.g. 1% for much of
> the EU sub-band). At ~170 ms per `kBinaryEvent`, that allows only a handful
> of sensor transmissions per minute on air, and each router rebroadcast counts
> too. Check the local rules before deploying.
