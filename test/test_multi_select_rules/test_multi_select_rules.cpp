#include <unity.h>

#include "multi_select_rules.h"

namespace {
ButtonEvent tap(Button b) {
  return ButtonEvent{b, false, false};
}

ButtonEvent longPress(Button b) {
  return ButtonEvent{b, false, true};
}

constexpr uint16_t VISIBLE = 4;
}  // namespace

void setUp() {}
void tearDown() {}

void test_short_ok_toggles_only_with_rows() {
  const MessageListModel top{};
  TEST_ASSERT_EQUAL(MultiSelectAction::Toggle, multiSelectOnButton(top, tap(Button::BTN3_OK), 3, VISIBLE, 0).action);
  TEST_ASSERT_EQUAL(MultiSelectAction::None, multiSelectOnButton(top, tap(Button::BTN3_OK), 0, VISIBLE, 0).action);
}

void test_up_down_move_without_wrapping() {
  const MessageListModel top{};
  TEST_ASSERT_EQUAL_UINT16(1, multiSelectOnButton(top, tap(Button::BTN2_DOWN), 3, VISIBLE, 0).model.selected);
  TEST_ASSERT_EQUAL_UINT16(0, multiSelectOnButton(top, tap(Button::BTN1_UP), 3, VISIBLE, 0).model.selected);
  const MessageListModel last{2, 0};
  TEST_ASSERT_EQUAL_UINT16(2, multiSelectOnButton(last, tap(Button::BTN2_DOWN), 3, VISIBLE, 0).model.selected);
}

void test_short_back_asks_to_cancel_only_with_checked_rows() {
  const MessageListModel top{};
  TEST_ASSERT_EQUAL(MultiSelectAction::Exit, multiSelectOnButton(top, tap(Button::BTN4_BACK), 3, VISIBLE, 0).action);
  TEST_ASSERT_EQUAL(MultiSelectAction::AskCancel,
                    multiSelectOnButton(top, tap(Button::BTN4_BACK), 3, VISIBLE, 2).action);
}

void test_long_press_event_is_left_to_the_hold() {
  const MessageListModel top{};
  TEST_ASSERT_EQUAL(MultiSelectAction::None, multiSelectOnButton(top, longPress(Button::BTN3_OK), 3, VISIBLE, 1).action);
  TEST_ASSERT_EQUAL(MultiSelectAction::None,
                    multiSelectOnButton(top, longPress(Button::BTN4_BACK), 3, VISIBLE, 1).action);
}

void test_hold_ok_opens_and_hold_back_asks_to_delete() {
  TEST_ASSERT_EQUAL(MultiSelectAction::Open, multiSelectOnHold(Button::BTN3_OK, 3, 0));
  TEST_ASSERT_EQUAL(MultiSelectAction::None, multiSelectOnHold(Button::BTN3_OK, 0, 0));
  TEST_ASSERT_EQUAL(MultiSelectAction::AskDelete, multiSelectOnHold(Button::BTN4_BACK, 3, 2));
  TEST_ASSERT_EQUAL(MultiSelectAction::None, multiSelectOnHold(Button::BTN4_BACK, 3, 0));
  TEST_ASSERT_EQUAL(MultiSelectAction::None, multiSelectOnHold(Button::BTN1_UP, 3, 2));
}

void test_toggle_and_count_checked() {
  bool checked[4] = {false, false, false, false};
  toggleChecked(checked, 4, 1);
  toggleChecked(checked, 4, 3);
  TEST_ASSERT_EQUAL_UINT16(2, countSelected(checked, 4));
  toggleChecked(checked, 4, 1);
  TEST_ASSERT_EQUAL_UINT16(1, countSelected(checked, 4));
  // Fuera de rango no toca nada
  toggleChecked(checked, 4, 9);
  TEST_ASSERT_EQUAL_UINT16(1, countSelected(checked, 4));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_short_ok_toggles_only_with_rows);
  RUN_TEST(test_up_down_move_without_wrapping);
  RUN_TEST(test_short_back_asks_to_cancel_only_with_checked_rows);
  RUN_TEST(test_long_press_event_is_left_to_the_hold);
  RUN_TEST(test_hold_ok_opens_and_hold_back_asks_to_delete);
  RUN_TEST(test_toggle_and_count_checked);
  return UNITY_END();
}
