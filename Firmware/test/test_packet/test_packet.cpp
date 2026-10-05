#include <stddef.h>
#include <stdint.h>
#include <unity.h>

#include "packet.h"

void setUp(void) {}
void tearDown(void) {}

static void test_header_size_is_locked(void) {
  TEST_ASSERT_EQUAL_UINT(9, sizeof(mesh::MeshHeader));
}

static void test_header_field_offsets(void) {
  TEST_ASSERT_EQUAL_UINT(0, offsetof(mesh::MeshHeader, version));
  TEST_ASSERT_EQUAL_UINT(1, offsetof(mesh::MeshHeader, network_id));
  TEST_ASSERT_EQUAL_UINT(2, offsetof(mesh::MeshHeader, type));
  TEST_ASSERT_EQUAL_UINT(3, offsetof(mesh::MeshHeader, hops_left));
  TEST_ASSERT_EQUAL_UINT(4, offsetof(mesh::MeshHeader, source));
  TEST_ASSERT_EQUAL_UINT(6, offsetof(mesh::MeshHeader, sequence));
  TEST_ASSERT_EQUAL_UINT(8, offsetof(mesh::MeshHeader, length));
}

static void test_max_payload_fits_header(void) {
  TEST_ASSERT_EQUAL_UINT(mesh::kMaxLength - 9, mesh::kMaxPayload);
  TEST_ASSERT_TRUE(mesh::kMaxPayload <= UINT8_MAX);
}

static void test_type_values_are_stable(void) {
  TEST_ASSERT_EQUAL_UINT(1, static_cast<uint8_t>(mesh::MeshType::kBinaryEvent));
  TEST_ASSERT_EQUAL_UINT(2, static_cast<uint8_t>(mesh::MeshType::kMeshBeacon));
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_header_size_is_locked);
  RUN_TEST(test_header_field_offsets);
  RUN_TEST(test_max_payload_fits_header);
  RUN_TEST(test_type_values_are_stable);
  return UNITY_END();
}
