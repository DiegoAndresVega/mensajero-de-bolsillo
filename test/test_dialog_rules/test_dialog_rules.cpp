#include <unity.h>

#include "dialog_rules.h"

namespace {
ButtonEvent tap(Button b) {
  return ButtonEvent{b, false, false};
}

ButtonEvent longPress(Button b) {
  return ButtonEvent{b, false, true};
}
}  // namespace

void setUp() {}
void tearDown() {}

void test_dialog_opens_and_closes() {
  TEST_ASSERT_FALSE(dialogIsOpen(DialogModel{}));
  TEST_ASSERT_TRUE(dialogIsOpen(dialogOpen(2)));
  const DialogStep no = dialogOnButton(dialogOpen(2), tap(Button::BTN4_BACK));
  TEST_ASSERT_FALSE(dialogIsOpen(no.model));
}

void test_double_confirmation_needs_two_yes() {
  const DialogStep first = dialogOnButton(dialogOpen(2), tap(Button::BTN3_OK));
  TEST_ASSERT_EQUAL(DialogResult::Pending, first.result);
  TEST_ASSERT_EQUAL_UINT8(1, first.model.step);
  const DialogStep second = dialogOnButton(first.model, tap(Button::BTN3_OK));
  TEST_ASSERT_EQUAL(DialogResult::Confirmed, second.result);
  TEST_ASSERT_FALSE(dialogIsOpen(second.model));
}

void test_no_on_any_step_cancels() {
  TEST_ASSERT_EQUAL(DialogResult::Cancelled, dialogOnButton(dialogOpen(2), tap(Button::BTN4_BACK)).result);
  const DialogModel second = dialogOnButton(dialogOpen(2), tap(Button::BTN3_OK)).model;
  TEST_ASSERT_EQUAL(DialogResult::Cancelled, dialogOnButton(second, tap(Button::BTN4_BACK)).result);
}

void test_single_step_dialog_confirms_at_once() {
  TEST_ASSERT_EQUAL(DialogResult::Confirmed, dialogOnButton(dialogOpen(1), tap(Button::BTN3_OK)).result);
}

void test_other_buttons_and_long_press_do_nothing() {
  const DialogModel open = dialogOpen(2);
  TEST_ASSERT_EQUAL(DialogResult::Pending, dialogOnButton(open, tap(Button::BTN1_UP)).result);
  TEST_ASSERT_EQUAL(DialogResult::Pending, dialogOnButton(open, longPress(Button::BTN3_OK)).result);
  TEST_ASSERT_EQUAL(DialogResult::Pending, dialogOnButton(open, longPress(Button::BTN4_BACK)).result);
  TEST_ASSERT_EQUAL_UINT8(0, dialogOnButton(open, tap(Button::BTN2_DOWN)).model.step);
  // Cerrado no reacciona
  TEST_ASSERT_EQUAL(DialogResult::Pending, dialogOnButton(DialogModel{}, tap(Button::BTN3_OK)).result);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_dialog_opens_and_closes);
  RUN_TEST(test_double_confirmation_needs_two_yes);
  RUN_TEST(test_no_on_any_step_cancels);
  RUN_TEST(test_single_step_dialog_confirms_at_once);
  RUN_TEST(test_other_buttons_and_long_press_do_nothing);
  return UNITY_END();
}
