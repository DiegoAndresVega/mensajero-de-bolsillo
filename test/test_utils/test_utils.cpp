#include <unity.h>

#include "utils.h"

void setUp() {}
void tearDown() {}

void test_backoff_doubles_from_base() {
  TEST_ASSERT_EQUAL_UINT32(1000, backoffDelayMs(0, 1000, 30000));
  TEST_ASSERT_EQUAL_UINT32(2000, backoffDelayMs(1, 1000, 30000));
  TEST_ASSERT_EQUAL_UINT32(4000, backoffDelayMs(2, 1000, 30000));
  TEST_ASSERT_EQUAL_UINT32(8000, backoffDelayMs(3, 1000, 30000));
  TEST_ASSERT_EQUAL_UINT32(16000, backoffDelayMs(4, 1000, 30000));
}

void test_backoff_caps_at_max() {
  TEST_ASSERT_EQUAL_UINT32(30000, backoffDelayMs(5, 1000, 30000));
  TEST_ASSERT_EQUAL_UINT32(30000, backoffDelayMs(200, 1000, 30000));
}

void test_valid_firestore_id_is_accepted() {
  TEST_ASSERT_TRUE(isValidMessageId("abc123", 40));
  TEST_ASSERT_TRUE(isValidMessageId("Xy7_-kLm9PqRsTuVwZ01", 40));
}

void test_invalid_ids_are_rejected() {
  TEST_ASSERT_FALSE(isValidMessageId(nullptr, 40));
  TEST_ASSERT_FALSE(isValidMessageId("", 40));
  TEST_ASSERT_FALSE(isValidMessageId("../config", 40));
  TEST_ASSERT_FALSE(isValidMessageId("a/b", 40));
  TEST_ASSERT_FALSE(isValidMessageId("con espacio", 40));
  TEST_ASSERT_FALSE(isValidMessageId("abcdef", 5));
}

void test_url_encode_keeps_unreserved_and_escapes_rest() {
  char out[3];
  TEST_ASSERT_EQUAL_size_t(1, urlEncodeChar('a', out));
  TEST_ASSERT_EQUAL_CHAR('a', out[0]);
  TEST_ASSERT_EQUAL_size_t(3, urlEncodeChar('&', out));
  TEST_ASSERT_EQUAL_MEMORY("%26", out, 3);
  TEST_ASSERT_EQUAL_size_t(3, urlEncodeChar('+', out));
  TEST_ASSERT_EQUAL_MEMORY("%2B", out, 3);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_backoff_doubles_from_base);
  RUN_TEST(test_backoff_caps_at_max);
  RUN_TEST(test_valid_firestore_id_is_accepted);
  RUN_TEST(test_invalid_ids_are_rejected);
  RUN_TEST(test_url_encode_keeps_unreserved_and_escapes_rest);
  return UNITY_END();
}
