#include <unity.h>

#include "info_rules.h"

namespace {
ButtonEvent tap(Button b) {
  return ButtonEvent{b, false, false};
}

constexpr size_t OUT_SIZE = 64;
}  // namespace

void setUp() {}
void tearDown() {}

// ----- Botones ------------------------------------------------------------------

void test_info_scrolls_within_limits_and_back_returns() {
  // 10 lineas, 4 visibles, pasos de 3: la primera visible va de 0 a 6
  TEST_ASSERT_EQUAL_UINT16(3, infoOnButton(0, tap(Button::BTN2_DOWN), 10, 4, 3).topLine);
  TEST_ASSERT_EQUAL_UINT16(6, infoOnButton(6, tap(Button::BTN2_DOWN), 10, 4, 3).topLine);
  TEST_ASSERT_EQUAL_UINT16(0, infoOnButton(2, tap(Button::BTN1_UP), 10, 4, 3).topLine);
  TEST_ASSERT_TRUE(infoOnButton(3, tap(Button::BTN4_BACK), 10, 4, 3).isBack);
}

void test_info_ignores_ok_and_long_press() {
  const InfoStep ok = infoOnButton(3, tap(Button::BTN3_OK), 10, 4, 3);
  TEST_ASSERT_EQUAL_UINT16(3, ok.topLine);
  TEST_ASSERT_FALSE(ok.isBack);
  TEST_ASSERT_FALSE(infoOnButton(3, ButtonEvent{Button::BTN4_BACK, false, true}, 10, 4, 3).isBack);
}

// ----- Mensaje personal -------------------------------------------------------------

void test_personal_message_converts_accents_and_uses_fallback_when_blank() {
  char out[OUT_SIZE];
  formatPersonalMessage("Hola Mar\xC3\xAD" "a", 500, "Sin mensaje", out, sizeof(out));
  TEST_ASSERT_EQUAL_STRING("Hola Mar\xED" "a", out);
  formatPersonalMessage("", 500, "Sin mensaje", out, sizeof(out));
  TEST_ASSERT_EQUAL_STRING("Sin mensaje", out);
  formatPersonalMessage("  \n ", 500, "Sin mensaje", out, sizeof(out));
  TEST_ASSERT_EQUAL_STRING("Sin mensaje", out);
  formatPersonalMessage(nullptr, 500, "Sin mensaje", out, sizeof(out));
  TEST_ASSERT_EQUAL_STRING("Sin mensaje", out);
}

void test_personal_message_is_truncated_with_ellipsis() {
  char out[OUT_SIZE];
  formatPersonalMessage("Un mensaje bastante largo", 12, "Sin mensaje", out, sizeof(out));
  TEST_ASSERT_EQUAL_STRING("Un mensaj...", out);
  formatPersonalMessage("Corto", 12, "Sin mensaje", out, sizeof(out));
  TEST_ASSERT_EQUAL_STRING("Corto", out);
  // Sin espacios antes de "..."
  formatPersonalMessage("Hola que tal", 8, "Sin mensaje", out, sizeof(out));
  TEST_ASSERT_EQUAL_STRING("Hola...", out);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_info_scrolls_within_limits_and_back_returns);
  RUN_TEST(test_info_ignores_ok_and_long_press);
  RUN_TEST(test_personal_message_converts_accents_and_uses_fallback_when_blank);
  RUN_TEST(test_personal_message_is_truncated_with_ellipsis);
  return UNITY_END();
}
