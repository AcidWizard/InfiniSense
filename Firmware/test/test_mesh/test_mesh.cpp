#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <unity.h>

#include "config.hpp"
#include "mesh.h"
#include "packet.h"
#include "radio.h"

radio::StubCapture radio::stub_last;

void setUp(void) {
  mesh::Begin();
  memset(&radio::stub_last, 0, sizeof(radio::stub_last));
}

void tearDown(void) {}

static void test_next_seq_increments(void) {
  const uint16_t first = mesh::NextSeq();
  const uint16_t second = mesh::NextSeq();
  TEST_ASSERT_EQUAL_UINT16(static_cast<uint16_t>(first + 1), second);
}

static void test_is_duplicate_tracks_remembered(void) {
  TEST_ASSERT_FALSE(mesh::IsDuplicate(0x1111, 1));
  mesh::Remember(0x1111, 1);
  TEST_ASSERT_TRUE(mesh::IsDuplicate(0x1111, 1));
  TEST_ASSERT_FALSE(mesh::IsDuplicate(0x1111, 2));
  TEST_ASSERT_FALSE(mesh::IsDuplicate(0x2222, 1));
}

static void test_dedup_ring_evicts_oldest(void) {
  for (uint16_t i = 1; i <= config::kMeshDedupSize; i++) {
    TEST_ASSERT_FALSE(mesh::IsDuplicate(0xABCD, i));
    mesh::Remember(0xABCD, i);
  }
  TEST_ASSERT_TRUE(mesh::IsDuplicate(0xABCD, 1));
  TEST_ASSERT_FALSE(mesh::IsDuplicate(0xABCD, config::kMeshDedupSize + 1));
  mesh::Remember(0xABCD, config::kMeshDedupSize + 1);
  TEST_ASSERT_FALSE(mesh::IsDuplicate(0xABCD, 1));
}

static void test_zero_pair_is_not_a_false_duplicate(void) {
  TEST_ASSERT_FALSE(mesh::IsDuplicate(0, 0));
  mesh::Remember(0, 0);
  TEST_ASSERT_TRUE(mesh::IsDuplicate(0, 0));
}

static void test_forward_flag_roundtrip(void) {
  TEST_ASSERT_FALSE(mesh::ForwardEnabled());
  mesh::SetForward(true);
  TEST_ASSERT_TRUE(mesh::ForwardEnabled());
}

static void test_build_rejects_bad_input(void) {
  uint8_t frame[mesh::kMaxLength];
  uint8_t payload[4] = {0};

  TEST_ASSERT_EQUAL_UINT(
      0, mesh::BuildBinaryEvent(nullptr, sizeof(frame), 1, 1, payload,
                                sizeof(payload), config::kMeshDefaultHops));
  TEST_ASSERT_EQUAL_UINT(
      0, mesh::BuildBinaryEvent(frame, sizeof(frame), 1, 1, nullptr, 1,
                                config::kMeshDefaultHops));
  TEST_ASSERT_EQUAL_UINT(
      0,
      mesh::BuildBinaryEvent(frame, sizeof(frame), 1, 1, payload,
                             mesh::kMaxPayload + 1, config::kMeshDefaultHops));
  TEST_ASSERT_EQUAL_UINT(
      0, mesh::BuildBinaryEvent(frame, sizeof(mesh::MeshHeader), 1, 1, payload,
                                sizeof(payload), config::kMeshDefaultHops));
}

static void test_build_serializes_header_and_payload(void) {
  uint8_t frame[mesh::kMaxLength];
  uint8_t payload[2] = {0xAB, 0xCD};

  const size_t length = mesh::BuildBinaryEvent(
      frame, sizeof(frame), 0x1234, 0x5678, payload, sizeof(payload), 3);
  TEST_ASSERT_EQUAL_UINT(sizeof(mesh::MeshHeader) + sizeof(payload), length);

  mesh::MeshHeader header;
  memcpy(&header, frame, sizeof(header));
  TEST_ASSERT_EQUAL_UINT8(mesh::kVersion, header.version);
  TEST_ASSERT_EQUAL_UINT8(config::kMeshNetId, header.network_id);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(mesh::MeshType::kBinaryEvent),
                          header.type);
  TEST_ASSERT_EQUAL_UINT8(3, header.hops_left);
  TEST_ASSERT_EQUAL_UINT16(0x1234, header.source);
  TEST_ASSERT_EQUAL_UINT16(0x5678, header.sequence);
  TEST_ASSERT_EQUAL_UINT8(sizeof(payload), header.length);
  TEST_ASSERT_EQUAL_UINT8(0xAB, frame[sizeof(mesh::MeshHeader)]);
  TEST_ASSERT_EQUAL_UINT8(0xCD, frame[sizeof(mesh::MeshHeader) + 1]);
}

static void test_handle_forwards_and_decrements_hops(void) {
  uint8_t frame[mesh::kMaxLength];
  uint8_t payload = 1;
  const size_t length =
      mesh::BuildBinaryEvent(frame, sizeof(frame), 0x1234, 0x5678, &payload, 1,
                             config::kMeshDefaultHops);

  mesh::SetForward(true);
  TEST_ASSERT_TRUE(mesh::Handle(frame, length));
  TEST_ASSERT_TRUE(radio::stub_last.sent);
  TEST_ASSERT_EQUAL_UINT(length, radio::stub_last.length);

  mesh::MeshHeader sent;
  memcpy(&sent, radio::stub_last.data, sizeof(sent));
  TEST_ASSERT_EQUAL_UINT8(config::kMeshDefaultHops - 1, sent.hops_left);
  TEST_ASSERT_EQUAL_UINT16(0x1234, sent.source);
  TEST_ASSERT_EQUAL_UINT16(0x5678, sent.sequence);
  TEST_ASSERT_EQUAL_UINT8(payload,
                          radio::stub_last.data[sizeof(mesh::MeshHeader)]);

  TEST_ASSERT_EQUAL_UINT8(1, mesh::PendingEvents());
  TEST_ASSERT_FALSE(mesh::Handle(frame, length));
  TEST_ASSERT_EQUAL_UINT8(1, mesh::PendingEvents());
}

static void test_handle_rejects_invalid(void) {
  uint8_t frame[mesh::kMaxLength];
  uint8_t payload = 1;
  const size_t length = mesh::BuildBinaryEvent(frame, sizeof(frame), 0x1234,
                                               0x5678, &payload, 1, 3);

  mesh::SetForward(true);

  TEST_ASSERT_FALSE(mesh::Handle(frame, sizeof(mesh::MeshHeader) - 1));

  uint8_t bad_version[mesh::kMaxLength];
  memcpy(bad_version, frame, length);
  bad_version[offsetof(mesh::MeshHeader, version)] = 0xFF;
  TEST_ASSERT_FALSE(mesh::Handle(bad_version, length));

  uint8_t bad_net[mesh::kMaxLength];
  memcpy(bad_net, frame, length);
  bad_net[offsetof(mesh::MeshHeader, network_id)] = 0xFF;
  TEST_ASSERT_FALSE(mesh::Handle(bad_net, length));

  uint8_t zero_hops[mesh::kMaxLength];
  memcpy(zero_hops, frame, length);
  zero_hops[offsetof(mesh::MeshHeader, hops_left)] = 0;
  TEST_ASSERT_FALSE(mesh::Handle(zero_hops, length));

  TEST_ASSERT_FALSE(mesh::Handle(frame, length - 1));
}

static void test_handle_consumes_when_forward_disabled(void) {
  uint8_t frame[mesh::kMaxLength];
  uint8_t payload = 1;
  const size_t length = mesh::BuildBinaryEvent(frame, sizeof(frame), 0x1234,
                                               0x5678, &payload, 1, 3);

  mesh::SetForward(false);
  TEST_ASSERT_TRUE(mesh::Handle(frame, length));
  TEST_ASSERT_FALSE(radio::stub_last.sent);
  TEST_ASSERT_EQUAL_UINT8(1, mesh::PendingEvents());

  mesh::Event event;
  TEST_ASSERT_TRUE(mesh::PopEvent(event));
  TEST_ASSERT_EQUAL_UINT16(0x1234, event.source);
  TEST_ASSERT_EQUAL_UINT16(0x5678, event.sequence);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(mesh::MeshType::kBinaryEvent),
                          static_cast<uint8_t>(event.type));
  TEST_ASSERT_EQUAL_UINT8(1, event.length);
  TEST_ASSERT_EQUAL_UINT8(payload, event.payload[0]);
  TEST_ASSERT_EQUAL_UINT8(0, mesh::PendingEvents());
  TEST_ASSERT_FALSE(mesh::PopEvent(event));
}

static void test_queue_evicts_oldest_when_full(void) {
  mesh::SetForward(false);
  uint8_t payload = 1;

  for (uint16_t i = 1; i <= config::kMeshEventQueueSize + 1; i++) {
    uint8_t frame[mesh::kMaxLength];
    const size_t length =
        mesh::BuildBinaryEvent(frame, sizeof(frame), 0x33, i, &payload, 1, 3);
    TEST_ASSERT_TRUE(mesh::Handle(frame, length));
  }

  TEST_ASSERT_EQUAL_UINT8(config::kMeshEventQueueSize, mesh::PendingEvents());
  mesh::Event event;
  TEST_ASSERT_TRUE(mesh::PopEvent(event));
  TEST_ASSERT_EQUAL_UINT16(2, event.sequence);
}

static void test_malformed_does_not_poison_dedup(void) {
  uint8_t frame[mesh::kMaxLength];
  uint8_t payload = 1;
  const size_t length = mesh::BuildBinaryEvent(frame, sizeof(frame), 0x5555,
                                               0x42, &payload, 1, 3);

  // Valid header fields, but a declared payload that is not actually present.
  uint8_t truncated[mesh::kMaxLength];
  memcpy(truncated, frame, length);
  truncated[offsetof(mesh::MeshHeader, length)] = 5;
  mesh::SetForward(false);
  TEST_ASSERT_FALSE(mesh::Handle(truncated, sizeof(mesh::MeshHeader)));

  // The same {source, sequence} must still be accepted afterwards.
  TEST_ASSERT_TRUE(mesh::Handle(frame, length));
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_next_seq_increments);
  RUN_TEST(test_is_duplicate_tracks_remembered);
  RUN_TEST(test_dedup_ring_evicts_oldest);
  RUN_TEST(test_zero_pair_is_not_a_false_duplicate);
  RUN_TEST(test_forward_flag_roundtrip);
  RUN_TEST(test_build_rejects_bad_input);
  RUN_TEST(test_build_serializes_header_and_payload);
  RUN_TEST(test_handle_forwards_and_decrements_hops);
  RUN_TEST(test_handle_rejects_invalid);
  RUN_TEST(test_handle_consumes_when_forward_disabled);
  RUN_TEST(test_queue_evicts_oldest_when_full);
  RUN_TEST(test_malformed_does_not_poison_dedup);
  return UNITY_END();
}
