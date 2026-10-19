#include <unity.h>

#include <vector>

#include "button_rules.h"

namespace {
constexpr ButtonTiming TIMING{30, 500, 500};
constexpr uint32_t SAMPLE_MS = 10;
constexpr uint8_t BTN1 = 1U << 0;
constexpr uint8_t BTN2 = 1U << 1;
constexpr uint8_t NONE = 0;

// Simula la tarea de botones: una muestra cada 10 ms con el mismo nivel
struct Sim {
  ButtonDecoder decoder{TIMING};
  uint32_t nowMs = 0;
  std::vector<ButtonEvent> events;

  void hold(uint8_t mask, uint32_t durationMs) {
    for (uint32_t t = 0; t < durationMs; t += SAMPLE_MS) {
      ButtonEvent ev;
      if (decoder.update(mask, nowMs, ev)) {
        events.push_back(ev);
      }
      nowMs += SAMPLE_MS;
    }
  }

  void tap(uint8_t mask, uint32_t pressMs = 80, uint32_t gapMs = 100) {
    hold(mask, pressMs);
    hold(NONE, gapMs);
  }
};
}  // namespace

void setUp() {}
void tearDown() {}

void test_bounce_shorter_than_debounce_is_ignored() {
  Sim sim;
  for (int i = 0; i < 5; ++i) {
    sim.hold(BTN1, 10);
    sim.hold(NONE, 10);
  }
  sim.hold(NONE, 600);
  TEST_ASSERT_EQUAL_size_t(0, sim.events.size());
}

void test_short_press_emits_single_tap_on_release() {
  Sim sim;
  sim.hold(BTN1, 100);
  TEST_ASSERT_EQUAL_size_t(0, sim.events.size());
  sim.hold(NONE, 100);
  TEST_ASSERT_EQUAL_size_t(1, sim.events.size());
  TEST_ASSERT_EQUAL(Button::BTN1_UP, sim.events[0].button);
  TEST_ASSERT_FALSE(sim.events[0].doubleTap);
  TEST_ASSERT_FALSE(sim.events[0].longPress);
}

void test_two_quick_taps_give_double_tap_on_second() {
  Sim sim;
  sim.tap(BTN2);
  sim.tap(BTN2);
  TEST_ASSERT_EQUAL_size_t(2, sim.events.size());
  TEST_ASSERT_FALSE(sim.events[0].doubleTap);
  TEST_ASSERT_TRUE(sim.events[1].doubleTap);
  TEST_ASSERT_EQUAL(Button::BTN2_DOWN, sim.events[1].button);
}

void test_taps_too_far_apart_are_not_double() {
  Sim sim;
  sim.tap(BTN1, 80, 600);
  sim.tap(BTN1);
  TEST_ASSERT_EQUAL_size_t(2, sim.events.size());
  TEST_ASSERT_FALSE(sim.events[1].doubleTap);
}

void test_triple_tap_gives_only_one_double() {
  Sim sim;
  sim.tap(BTN1);
  sim.tap(BTN1);
  sim.tap(BTN1);
  TEST_ASSERT_EQUAL_size_t(3, sim.events.size());
  TEST_ASSERT_TRUE(sim.events[1].doubleTap);
  TEST_ASSERT_FALSE(sim.events[2].doubleTap);
}

void test_taps_on_different_buttons_are_not_double() {
  Sim sim;
  sim.tap(BTN1);
  sim.tap(BTN2);
  TEST_ASSERT_EQUAL_size_t(2, sim.events.size());
  TEST_ASSERT_FALSE(sim.events[1].doubleTap);
}

void test_long_press_fires_once_while_held_and_nothing_on_release() {
  Sim sim;
  sim.hold(BTN1, 700);
  TEST_ASSERT_EQUAL_size_t(1, sim.events.size());
  TEST_ASSERT_TRUE(sim.events[0].longPress);
  sim.hold(BTN1, 500);
  sim.hold(NONE, 100);
  TEST_ASSERT_EQUAL_size_t(1, sim.events.size());
}

void test_long_press_does_not_count_as_first_tap_of_double() {
  Sim sim;
  sim.hold(BTN1, 600);
  sim.hold(NONE, 50);
  sim.tap(BTN1);
  TEST_ASSERT_EQUAL_size_t(2, sim.events.size());
  TEST_ASSERT_FALSE(sim.events[1].doubleTap);
}

void test_second_button_while_first_held_is_ignored() {
  Sim sim;
  sim.hold(BTN1, 50);
  sim.hold(BTN1 | BTN2, 100);
  sim.hold(BTN1, 50);
  sim.hold(NONE, 100);
  TEST_ASSERT_EQUAL_size_t(1, sim.events.size());
  TEST_ASSERT_EQUAL(Button::BTN1_UP, sim.events[0].button);
}

void test_button_still_held_after_first_released_is_ignored() {
  Sim sim;
  sim.hold(BTN1, 50);
  sim.hold(BTN1 | BTN2, 100);
  sim.hold(BTN2, 100);
  sim.hold(NONE, 100);
  TEST_ASSERT_EQUAL_size_t(1, sim.events.size());
  TEST_ASSERT_EQUAL(Button::BTN1_UP, sim.events[0].button);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_bounce_shorter_than_debounce_is_ignored);
  RUN_TEST(test_short_press_emits_single_tap_on_release);
  RUN_TEST(test_two_quick_taps_give_double_tap_on_second);
  RUN_TEST(test_taps_too_far_apart_are_not_double);
  RUN_TEST(test_triple_tap_gives_only_one_double);
  RUN_TEST(test_taps_on_different_buttons_are_not_double);
  RUN_TEST(test_long_press_fires_once_while_held_and_nothing_on_release);
  RUN_TEST(test_long_press_does_not_count_as_first_tap_of_double);
  RUN_TEST(test_second_button_while_first_held_is_ignored);
  RUN_TEST(test_button_still_held_after_first_released_is_ignored);
  return UNITY_END();
}
