#include <unity.h>

#include "trash_menu_rules.h"

namespace {
ButtonEvent tap(Button b) {
  return ButtonEvent{b, false, false};
}

ButtonEvent longPress(Button b) {
  return ButtonEvent{b, false, true};
}

constexpr auto CANCEL_ROW = static_cast<uint8_t>(TrashMenuItem::CANCEL_PENDING);
}  // namespace

void setUp() {}
void tearDown() {}

void test_menu_has_three_items_or_four_with_pending_queue() {
  TEST_ASSERT_EQUAL_UINT8(3, trashMenuItemCount(0));
  TEST_ASSERT_EQUAL_UINT8(4, trashMenuItemCount(1));
  TEST_ASSERT_EQUAL_UINT8(4, trashMenuItemCount(25));
}

void test_menu_moves_with_wraparound() {
  TEST_ASSERT_EQUAL_UINT8(2, trashMenuOnButton(0, tap(Button::BTN1_UP), 0).selected);
  TEST_ASSERT_EQUAL_UINT8(3, trashMenuOnButton(0, tap(Button::BTN1_UP), 2).selected);
  TEST_ASSERT_EQUAL_UINT8(0, trashMenuOnButton(2, tap(Button::BTN2_DOWN), 0).selected);
  TEST_ASSERT_EQUAL_UINT8(3, trashMenuOnButton(2, tap(Button::BTN2_DOWN), 2).selected);
}

void test_ok_opens_each_option() {
  TEST_ASSERT_EQUAL(TrashMenuAction::OpenList, trashMenuOnButton(0, tap(Button::BTN3_OK), 0).action);
  TEST_ASSERT_EQUAL(TrashMenuAction::OpenSelect, trashMenuOnButton(1, tap(Button::BTN3_OK), 0).action);
  TEST_ASSERT_EQUAL(TrashMenuAction::AskEmpty, trashMenuOnButton(2, tap(Button::BTN3_OK), 0).action);
  TEST_ASSERT_EQUAL(TrashMenuAction::AskCancelQueue, trashMenuOnButton(CANCEL_ROW, tap(Button::BTN3_OK), 3).action);
}

void test_back_exits_and_long_press_does_nothing() {
  TEST_ASSERT_EQUAL(TrashMenuAction::Exit, trashMenuOnButton(1, tap(Button::BTN4_BACK), 0).action);
  TEST_ASSERT_EQUAL(TrashMenuAction::None, trashMenuOnButton(1, longPress(Button::BTN3_OK), 0).action);
  TEST_ASSERT_EQUAL(TrashMenuAction::None, trashMenuOnButton(1, longPress(Button::BTN4_BACK), 0).action);
}

void test_cancel_row_disappears_when_queue_empties() {
  TEST_ASSERT_EQUAL_UINT8(2, trashMenuClamp(CANCEL_ROW, 0));
  TEST_ASSERT_EQUAL_UINT8(CANCEL_ROW, trashMenuClamp(CANCEL_ROW, 4));
  // En la fila que ya no existe, 3 hace lo de la ultima que queda
  TEST_ASSERT_EQUAL(TrashMenuAction::AskEmpty, trashMenuOnButton(CANCEL_ROW, tap(Button::BTN3_OK), 0).action);
}

void test_empty_always_confirms_unless_nothing_at_all() {
  TEST_ASSERT_EQUAL(TrashEmptyDecision::Confirm, trashEmptyDecision(3, 0));
  // Todo ya en la cola (se vacio sin conexion): tambien pregunta y reintenta
  TEST_ASSERT_EQUAL(TrashEmptyDecision::Confirm, trashEmptyDecision(0, 2));
  TEST_ASSERT_EQUAL(TrashEmptyDecision::NothingToEmpty, trashEmptyDecision(0, 0));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_menu_has_three_items_or_four_with_pending_queue);
  RUN_TEST(test_menu_moves_with_wraparound);
  RUN_TEST(test_ok_opens_each_option);
  RUN_TEST(test_back_exits_and_long_press_does_nothing);
  RUN_TEST(test_cancel_row_disappears_when_queue_empties);
  RUN_TEST(test_empty_always_confirms_unless_nothing_at_all);
  return UNITY_END();
}
