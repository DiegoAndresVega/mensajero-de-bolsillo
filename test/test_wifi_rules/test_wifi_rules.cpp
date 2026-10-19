#include <unity.h>

#include "wifi_rules.h"

namespace {
WifiParseResult parse(const char* json) {
  JsonDocument doc;
  deserializeJson(doc, json);
  return wifiNetworksFromJson(doc.as<JsonVariantConst>());
}
}  // namespace

void setUp() {}
void tearDown() {}

// ----- Siguiente red --------------------------------------------------------------

void test_next_index_with_one_network_stays_at_zero() {
  TEST_ASSERT_EQUAL_UINT8(0, wifiNextIndex(0, 1));
}

void test_next_index_with_two_and_three_networks_wraps_to_zero() {
  TEST_ASSERT_EQUAL_UINT8(1, wifiNextIndex(0, 2));
  TEST_ASSERT_EQUAL_UINT8(0, wifiNextIndex(1, 2));
  TEST_ASSERT_EQUAL_UINT8(1, wifiNextIndex(0, 3));
  TEST_ASSERT_EQUAL_UINT8(2, wifiNextIndex(1, 3));
  TEST_ASSERT_EQUAL_UINT8(0, wifiNextIndex(2, 3));
}

void test_next_index_with_five_networks_visits_all_and_wraps() {
  uint8_t index = 0;
  for (uint8_t expected = 1; expected < 5; ++expected) {
    index = wifiNextIndex(index, 5);
    TEST_ASSERT_EQUAL_UINT8(expected, index);
  }
  TEST_ASSERT_EQUAL_UINT8(0, wifiNextIndex(index, 5));
}

void test_next_index_without_networks_or_out_of_range_is_zero() {
  TEST_ASSERT_EQUAL_UINT8(0, wifiNextIndex(0, 0));
  TEST_ASSERT_EQUAL_UINT8(0, wifiNextIndex(7, 3));
}

// ----- Primera red de cada vuelta -------------------------------------------------

void test_first_index_without_previous_connection_is_zero() {
  TEST_ASSERT_EQUAL_UINT8(0, wifiFirstIndex(WIFI_NO_LAST_GOOD, 3));
}

void test_first_index_starts_with_last_network_that_worked() {
  TEST_ASSERT_EQUAL_UINT8(2, wifiFirstIndex(2, 3));
  TEST_ASSERT_EQUAL_UINT8(1, wifiFirstIndex(1, 3));
}

void test_first_index_out_of_range_or_without_networks_is_zero() {
  TEST_ASSERT_EQUAL_UINT8(0, wifiFirstIndex(4, 3));
  TEST_ASSERT_EQUAL_UINT8(0, wifiFirstIndex(0, 0));
}

// ----- Espera tras fallar todas ------------------------------------------------------

void test_waits_30s_after_all_networks_failed() {
  const uint32_t failedMs = 1000;
  TEST_ASSERT_TRUE(wifiShouldWaitAllFailed(true, failedMs, failedMs));
  TEST_ASSERT_TRUE(wifiShouldWaitAllFailed(true, failedMs, failedMs + WIFI_ALL_FAILED_DELAY_MS - 1));
  TEST_ASSERT_FALSE(wifiShouldWaitAllFailed(true, failedMs, failedMs + WIFI_ALL_FAILED_DELAY_MS));
  TEST_ASSERT_EQUAL_UINT32(WIFI_ALL_FAILED_DELAY_MS - 500, wifiAllFailedRemainingMs(true, failedMs, failedMs + 500));
  TEST_ASSERT_EQUAL_UINT32(0, wifiAllFailedRemainingMs(true, failedMs, failedMs + WIFI_ALL_FAILED_DELAY_MS + 1));
}

void test_does_not_wait_if_nothing_failed() {
  TEST_ASSERT_FALSE(wifiShouldWaitAllFailed(false, 0, 0));
  TEST_ASSERT_EQUAL_UINT32(0, wifiAllFailedRemainingMs(false, 0, 0));
}

void test_wait_survives_millis_wraparound() {
  const uint32_t failedMs = 0xFFFFFFFFu - 1000;
  TEST_ASSERT_TRUE(wifiShouldWaitAllFailed(true, failedMs, 5000));
  TEST_ASSERT_FALSE(wifiShouldWaitAllFailed(true, failedMs, WIFI_ALL_FAILED_DELAY_MS));
}

void test_scan_max_ms_is_one_timeout_per_network() {
  TEST_ASSERT_EQUAL_UINT32(WIFI_TRY_TIMEOUT_MS, wifiScanMaxMs(0));
  TEST_ASSERT_EQUAL_UINT32(WIFI_TRY_TIMEOUT_MS, wifiScanMaxMs(1));
  TEST_ASSERT_EQUAL_UINT32(3 * WIFI_TRY_TIMEOUT_MS, wifiScanMaxMs(3));
}

// ----- Parser de config.json ----------------------------------------------------------

void test_parser_reads_new_format_array_in_order() {
  const WifiParseResult r = parse(
      R"({"wifi_networks":[{"ssid":"Casa","password":"a1"},{"ssid":"Trabajo","password":"b2"},)"
      R"({"ssid":"Movil","password":"c3"}]})");
  TEST_ASSERT_EQUAL_UINT8(3, r.count);
  TEST_ASSERT_FALSE(r.isLegacy);
  TEST_ASSERT_EQUAL_STRING("Casa", r.networks[0].ssid);
  TEST_ASSERT_EQUAL_STRING("a1", r.networks[0].password);
  TEST_ASSERT_EQUAL_STRING("Movil", r.networks[2].ssid);
  TEST_ASSERT_EQUAL_STRING("c3", r.networks[2].password);
}

void test_parser_accepts_old_format_as_single_network() {
  const WifiParseResult r = parse(R"({"wifi_ssid":"MiWiFi","wifi_password":"12345678"})");
  TEST_ASSERT_EQUAL_UINT8(1, r.count);
  TEST_ASSERT_TRUE(r.isLegacy);
  TEST_ASSERT_EQUAL_STRING("MiWiFi", r.networks[0].ssid);
  TEST_ASSERT_EQUAL_STRING("12345678", r.networks[0].password);
}

void test_parser_prefers_array_over_old_fields() {
  const WifiParseResult r =
      parse(R"({"wifi_ssid":"Vieja","wifi_password":"x","wifi_networks":[{"ssid":"Nueva","password":"y"}]})");
  TEST_ASSERT_EQUAL_UINT8(1, r.count);
  TEST_ASSERT_FALSE(r.isLegacy);
  TEST_ASSERT_EQUAL_STRING("Nueva", r.networks[0].ssid);
}

void test_parser_keeps_first_max_networks_and_counts_extra() {
  const WifiParseResult r = parse(
      R"({"wifi_networks":[{"ssid":"r1"},{"ssid":"r2"},{"ssid":"r3"},{"ssid":"r4"},{"ssid":"r5"},)"
      R"({"ssid":"r6"},{"ssid":"r7"}]})");
  TEST_ASSERT_EQUAL_UINT8(WIFI_MAX_NETWORKS, r.count);
  TEST_ASSERT_EQUAL_UINT8(2, r.extra);
  TEST_ASSERT_EQUAL_STRING("r5", r.networks[4].ssid);
}

void test_parser_skips_invalid_entries() {
  // Sin ssid, ssid vacio, ssid de 33, password de 65 y una entrada que no es objeto
  const WifiParseResult r = parse(
      R"({"wifi_networks":[{"password":"p"},{"ssid":""},{"ssid":"123456789012345678901234567890123"},)"
      R"({"ssid":"Larga","password":"12345678901234567890123456789012345678901234567890123456789012345"},)"
      R"("Suelta",{"ssid":"Buena"}]})");
  TEST_ASSERT_EQUAL_UINT8(1, r.count);
  TEST_ASSERT_EQUAL_UINT8(5, r.ignored);
  TEST_ASSERT_EQUAL_STRING("Buena", r.networks[0].ssid);
  // Sin password: red abierta
  TEST_ASSERT_EQUAL_STRING("", r.networks[0].password);
}

void test_parser_accepts_max_lengths() {
  const WifiParseResult r = parse(
      R"({"wifi_networks":[{"ssid":"12345678901234567890123456789012",)"
      R"("password":"1234567890123456789012345678901234567890123456789012345678901234"}]})");
  TEST_ASSERT_EQUAL_UINT8(1, r.count);
  TEST_ASSERT_EQUAL_size_t(WIFI_SSID_MAX_LEN, strlen(r.networks[0].ssid));
  TEST_ASSERT_EQUAL_size_t(WIFI_PASSWORD_MAX_LEN, strlen(r.networks[0].password));
}

void test_parser_without_networks_gives_zero() {
  TEST_ASSERT_EQUAL_UINT8(0, parse(R"({"api_url":"https://x"})").count);
  TEST_ASSERT_EQUAL_UINT8(0, parse(R"({"wifi_networks":[]})").count);
  TEST_ASSERT_EQUAL_UINT8(0, parse(R"({"wifi_ssid":""})").count);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_next_index_with_one_network_stays_at_zero);
  RUN_TEST(test_next_index_with_two_and_three_networks_wraps_to_zero);
  RUN_TEST(test_next_index_with_five_networks_visits_all_and_wraps);
  RUN_TEST(test_next_index_without_networks_or_out_of_range_is_zero);
  RUN_TEST(test_first_index_without_previous_connection_is_zero);
  RUN_TEST(test_first_index_starts_with_last_network_that_worked);
  RUN_TEST(test_first_index_out_of_range_or_without_networks_is_zero);
  RUN_TEST(test_waits_30s_after_all_networks_failed);
  RUN_TEST(test_does_not_wait_if_nothing_failed);
  RUN_TEST(test_wait_survives_millis_wraparound);
  RUN_TEST(test_scan_max_ms_is_one_timeout_per_network);
  RUN_TEST(test_parser_reads_new_format_array_in_order);
  RUN_TEST(test_parser_accepts_old_format_as_single_network);
  RUN_TEST(test_parser_prefers_array_over_old_fields);
  RUN_TEST(test_parser_keeps_first_max_networks_and_counts_extra);
  RUN_TEST(test_parser_skips_invalid_entries);
  RUN_TEST(test_parser_accepts_max_lengths);
  RUN_TEST(test_parser_without_networks_gives_zero);
  return UNITY_END();
}
