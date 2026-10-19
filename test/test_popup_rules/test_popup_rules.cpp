#include <unity.h>

#include "popup_rules.h"

namespace {
constexpr uint32_t HZ = 2;
constexpr int32_t AMPLITUDE = 2;
constexpr uint32_t DURATION_MS = 3000;
}  // namespace

void setUp() {}
void tearDown() {}

void test_wobble_starts_centered_and_swings_both_ways() {
  TEST_ASSERT_EQUAL_INT32(0, popupWobbleX(0, HZ, AMPLITUDE));
  // A 2 Hz una vuelta dura 500 ms: arriba del todo a 125 ms, abajo a 375 ms
  TEST_ASSERT_EQUAL_INT32(AMPLITUDE, popupWobbleX(125, HZ, AMPLITUDE));
  TEST_ASSERT_EQUAL_INT32(-AMPLITUDE, popupWobbleX(375, HZ, AMPLITUDE));
  TEST_ASSERT_EQUAL_INT32(0, popupWobbleX(500, HZ, AMPLITUDE));
}

void test_wobble_never_exceeds_amplitude() {
  for (uint32_t ms = 0; ms <= DURATION_MS; ms += 7) {
    const int32_t x = popupWobbleX(ms, HZ, AMPLITUDE);
    TEST_ASSERT_TRUE(x >= -AMPLITUDE && x <= AMPLITUDE);
  }
}

void test_popup_lasts_exactly_its_duration_even_across_wraparound() {
  TEST_ASSERT_FALSE(isPopupOver(1000, 3999, DURATION_MS));
  TEST_ASSERT_TRUE(isPopupOver(1000, 4000, DURATION_MS));
  TEST_ASSERT_FALSE(isPopupOver(0xFFFFFF00UL, 0x00000100UL, DURATION_MS));
}

void test_back_cancels_popup_short_double_or_long() {
  TEST_ASSERT_TRUE(popupCancelsOn(ButtonEvent{Button::BTN4_BACK, false, false}));
  TEST_ASSERT_TRUE(popupCancelsOn(ButtonEvent{Button::BTN4_BACK, true, false}));
  // Mantenido mas de medio segundo llega como pulsacion larga: tambien cancela
  TEST_ASSERT_TRUE(popupCancelsOn(ButtonEvent{Button::BTN4_BACK, false, true}));
  TEST_ASSERT_FALSE(popupCancelsOn(ButtonEvent{Button::BTN3_OK, false, false}));
  TEST_ASSERT_FALSE(popupCancelsOn(ButtonEvent{Button::BTN1_UP, false, true}));
}

void test_popup_waits_for_open_dialog_and_skips_boot() {
  TEST_ASSERT_EQUAL(PopupStart::Show, popupStartFor(true, false, false));
  TEST_ASSERT_EQUAL(PopupStart::Defer, popupStartFor(true, false, true));
  TEST_ASSERT_EQUAL(PopupStart::None, popupStartFor(true, true, false));
  TEST_ASSERT_EQUAL(PopupStart::None, popupStartFor(false, false, false));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_wobble_starts_centered_and_swings_both_ways);
  RUN_TEST(test_wobble_never_exceeds_amplitude);
  RUN_TEST(test_popup_lasts_exactly_its_duration_even_across_wraparound);
  RUN_TEST(test_back_cancels_popup_short_double_or_long);
  RUN_TEST(test_popup_waits_for_open_dialog_and_skips_boot);
  return UNITY_END();
}
