#include <unity.h>

#include "download_rules.h"

namespace {
constexpr size_t MAX_BYTES = 1000;
constexpr uint32_t STALL_MS = 10000;
constexpr int32_t UNKNOWN_SIZE = -1;

DownloadStatus statusAt(size_t received, int32_t expected, uint32_t lastDataMs, uint32_t nowMs,
                        bool isOpen = true) {
  return downloadStatus({received, expected, lastDataMs, nowMs, isOpen}, MAX_BYTES, STALL_MS);
}
}  // namespace

void setUp() {}
void tearDown() {}

// ----- Timeout de progreso ------------------------------------------------------

void test_stalls_after_10s_without_data() {
  TEST_ASSERT_EQUAL(DownloadStatus::InProgress, statusAt(0, 500, 0, STALL_MS - 1));
  TEST_ASSERT_EQUAL(DownloadStatus::Stalled, statusAt(0, 500, 0, STALL_MS));
  TEST_ASSERT_EQUAL(DownloadStatus::Stalled, statusAt(100, UNKNOWN_SIZE, 0, STALL_MS + 1));
}

void test_new_data_restarts_the_stall_timer() {
  // Llegaron datos a los 9 s: a los 15 s aun no han pasado 10 s sin nada
  TEST_ASSERT_EQUAL(DownloadStatus::InProgress, statusAt(200, 500, 9000, 15000));
  TEST_ASSERT_EQUAL(DownloadStatus::Stalled, statusAt(200, 500, 9000, 9000 + STALL_MS));
}

void test_stall_timer_survives_millis_wraparound() {
  const uint32_t lastMs = 0xFFFFFFFFu - 1000;
  TEST_ASSERT_EQUAL(DownloadStatus::InProgress, statusAt(10, 500, lastMs, 5000));
  TEST_ASSERT_EQUAL(DownloadStatus::Stalled, statusAt(10, 500, lastMs, STALL_MS));
}

// ----- Fin de la descarga ----------------------------------------------------------

void test_completes_at_content_length() {
  TEST_ASSERT_EQUAL(DownloadStatus::InProgress, statusAt(499, 500, 0, 10));
  TEST_ASSERT_EQUAL(DownloadStatus::Complete, statusAt(500, 500, 0, 10));
  // Aunque el servidor siga con la conexion abierta
  TEST_ASSERT_EQUAL(DownloadStatus::Complete, statusAt(500, 500, 0, 10, true));
}

void test_closed_before_content_length_is_truncated() {
  TEST_ASSERT_EQUAL(DownloadStatus::Truncated, statusAt(300, 500, 0, 10, false));
}

void test_unknown_size_completes_when_server_closes() {
  TEST_ASSERT_EQUAL(DownloadStatus::InProgress, statusAt(300, UNKNOWN_SIZE, 0, 10, true));
  TEST_ASSERT_EQUAL(DownloadStatus::Complete, statusAt(300, UNKNOWN_SIZE, 0, 10, false));
  // Cerrada sin haber mandado nada: no es un MP3
  TEST_ASSERT_EQUAL(DownloadStatus::Truncated, statusAt(0, UNKNOWN_SIZE, 0, 10, false));
}

void test_zero_content_length_is_truncated() {
  TEST_ASSERT_EQUAL(DownloadStatus::Truncated, statusAt(0, 0, 0, 10));
}

// ----- Limite de tamano ----------------------------------------------------------

void test_unknown_size_over_max_is_too_big() {
  TEST_ASSERT_EQUAL(DownloadStatus::InProgress, statusAt(MAX_BYTES, UNKNOWN_SIZE, 0, 10));
  TEST_ASSERT_EQUAL(DownloadStatus::TooBig, statusAt(MAX_BYTES + 1, UNKNOWN_SIZE, 0, 10));
  TEST_ASSERT_EQUAL(DownloadStatus::TooBig, statusAt(MAX_BYTES + 1, UNKNOWN_SIZE, 0, 10, false));
}

void test_content_length_over_max_is_too_big_before_reading() {
  TEST_ASSERT_EQUAL(DownloadStatus::TooBig, statusAt(0, MAX_BYTES + 1, 0, 0));
  TEST_ASSERT_EQUAL(DownloadStatus::InProgress, statusAt(0, MAX_BYTES, 0, 0));
}

// ----- Tamano de cada lectura ---------------------------------------------------

void test_chunk_is_limited_by_available_buffer_and_remaining() {
  TEST_ASSERT_EQUAL_size_t(100, downloadChunkLen(100, 4096, 0, UNKNOWN_SIZE));
  TEST_ASSERT_EQUAL_size_t(4096, downloadChunkLen(9000, 4096, 0, UNKNOWN_SIZE));
  // Faltan 50 para Content-Length: no se lee mas
  TEST_ASSERT_EQUAL_size_t(50, downloadChunkLen(9000, 4096, 450, 500));
  TEST_ASSERT_EQUAL_size_t(0, downloadChunkLen(9000, 4096, 500, 500));
  TEST_ASSERT_EQUAL_size_t(0, downloadChunkLen(0, 4096, 0, 500));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_stalls_after_10s_without_data);
  RUN_TEST(test_new_data_restarts_the_stall_timer);
  RUN_TEST(test_stall_timer_survives_millis_wraparound);
  RUN_TEST(test_completes_at_content_length);
  RUN_TEST(test_closed_before_content_length_is_truncated);
  RUN_TEST(test_unknown_size_completes_when_server_closes);
  RUN_TEST(test_zero_content_length_is_truncated);
  RUN_TEST(test_unknown_size_over_max_is_too_big);
  RUN_TEST(test_content_length_over_max_is_too_big_before_reading);
  RUN_TEST(test_chunk_is_limited_by_available_buffer_and_remaining);
  return UNITY_END();
}
