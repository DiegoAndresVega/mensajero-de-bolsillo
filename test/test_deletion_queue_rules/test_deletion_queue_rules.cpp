#include <unity.h>

#include "deletion_queue_rules.h"

namespace {
// La cola ocupa ~8 KB: fuera de la pila de los tests
DeletionQueueIds queue;
}  // namespace

void setUp() {
  queueClear(queue);
}
void tearDown() {}

void test_add_keeps_order_and_ignores_duplicates() {
  TEST_ASSERT_TRUE(queueAdd(queue, "abc"));
  TEST_ASSERT_TRUE(queueAdd(queue, "def"));
  TEST_ASSERT_TRUE(queueAdd(queue, "abc"));
  TEST_ASSERT_EQUAL_UINT16(2, queue.count);
  TEST_ASSERT_EQUAL_STRING("abc", queue.ids[0]);
  TEST_ASSERT_EQUAL_STRING("def", queue.ids[1]);
}

void test_add_rejects_invalid_ids() {
  TEST_ASSERT_FALSE(queueAdd(queue, "../config"));
  TEST_ASSERT_FALSE(queueAdd(queue, ""));
  TEST_ASSERT_FALSE(queueAdd(queue, nullptr));
  TEST_ASSERT_EQUAL_UINT16(0, queue.count);
}

void test_add_stops_at_capacity() {
  char id[8];
  for (size_t i = 0; i < DELETION_QUEUE_CAPACITY; ++i) {
    snprintf(id, sizeof(id), "id%u", static_cast<unsigned>(i));
    TEST_ASSERT_TRUE(queueAdd(queue, id));
  }
  TEST_ASSERT_FALSE(queueAdd(queue, "extra"));
  TEST_ASSERT_EQUAL_UINT16(DELETION_QUEUE_CAPACITY, queue.count);
}

void test_remove_from_middle_keeps_the_rest_in_order() {
  queueAdd(queue, "a");
  queueAdd(queue, "b");
  queueAdd(queue, "c");
  TEST_ASSERT_TRUE(queueRemove(queue, "b"));
  TEST_ASSERT_EQUAL_UINT16(2, queue.count);
  TEST_ASSERT_EQUAL_STRING("a", queue.ids[0]);
  TEST_ASSERT_EQUAL_STRING("c", queue.ids[1]);
  TEST_ASSERT_FALSE(queueRemove(queue, "b"));
}

void test_contains_and_clear() {
  queueAdd(queue, "a");
  TEST_ASSERT_TRUE(queueContains(queue, "a"));
  TEST_ASSERT_FALSE(queueContains(queue, "z"));
  TEST_ASSERT_FALSE(queueContains(queue, nullptr));
  queueClear(queue);
  TEST_ASSERT_EQUAL_UINT16(0, queue.count);
  TEST_ASSERT_FALSE(queueContains(queue, "a"));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_add_keeps_order_and_ignores_duplicates);
  RUN_TEST(test_add_rejects_invalid_ids);
  RUN_TEST(test_add_stops_at_capacity);
  RUN_TEST(test_remove_from_middle_keeps_the_rest_in_order);
  RUN_TEST(test_contains_and_clear);
  return UNITY_END();
}
