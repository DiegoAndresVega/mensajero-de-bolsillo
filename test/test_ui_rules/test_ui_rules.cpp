#include <unity.h>

#include "ui_rules.h"

namespace {
constexpr UiTimeouts TIMEOUTS{2000, 10000, 60000, 30000};

ButtonEvent tap(Button b) {
  return ButtonEvent{b, false, false};
}

ButtonEvent doubleTap(Button b) {
  return ButtonEvent{b, true, false};
}

ButtonEvent longPress(Button b) {
  return ButtonEvent{b, false, true};
}
}  // namespace

void setUp() {}
void tearDown() {}

// ----- Estados ----------------------------------------------------------------

void test_boot_goes_to_eyes_without_input_and_menu_with_input() {
  TEST_ASSERT_EQUAL(UiState::BOOT, uiStateAfterTime(UiState::BOOT, 1999, 1999, false, TIMEOUTS));
  TEST_ASSERT_EQUAL(UiState::EYES, uiStateAfterTime(UiState::BOOT, 2000, 2000, false, TIMEOUTS));
  TEST_ASSERT_EQUAL(UiState::MENU, uiStateAfterTime(UiState::BOOT, 2000, 100, true, TIMEOUTS));
}

void test_menu_and_message_screens_go_to_eyes_after_idle() {
  TEST_ASSERT_EQUAL(UiState::MENU, uiStateAfterTime(UiState::MENU, 90000, 9999, false, TIMEOUTS));
  TEST_ASSERT_EQUAL(UiState::EYES, uiStateAfterTime(UiState::MENU, 90000, 10000, false, TIMEOUTS));
  TEST_ASSERT_EQUAL(UiState::EYES, uiStateAfterTime(UiState::MESSAGE_LIST, 0, 10000, false, TIMEOUTS));
}

void test_message_view_waits_longer_before_eyes() {
  TEST_ASSERT_EQUAL(UiState::MESSAGE_VIEW, uiStateAfterTime(UiState::MESSAGE_VIEW, 0, 29999, false, TIMEOUTS));
  TEST_ASSERT_EQUAL(UiState::EYES, uiStateAfterTime(UiState::MESSAGE_VIEW, 0, 30000, false, TIMEOUTS));
}

void test_audio_view_waits_like_text_view_and_takes_buttons() {
  TEST_ASSERT_EQUAL(UiState::AUDIO_VIEW, uiStateAfterTime(UiState::AUDIO_VIEW, 0, 29999, false, TIMEOUTS));
  TEST_ASSERT_EQUAL(UiState::EYES, uiStateAfterTime(UiState::AUDIO_VIEW, 0, 30000, false, TIMEOUTS));
  TEST_ASSERT_EQUAL(UiButtonRoute::Screen, uiRouteButton(UiState::AUDIO_VIEW, tap(Button::BTN3_OK)));
}

void test_settings_goes_to_eyes_after_menu_idle_and_takes_buttons() {
  TEST_ASSERT_EQUAL(UiState::SETTINGS, uiStateAfterTime(UiState::SETTINGS, 0, 9999, false, TIMEOUTS));
  TEST_ASSERT_EQUAL(UiState::EYES, uiStateAfterTime(UiState::SETTINGS, 0, 10000, false, TIMEOUTS));
  TEST_ASSERT_EQUAL(UiButtonRoute::Screen, uiRouteButton(UiState::SETTINGS, tap(Button::BTN1_UP)));
}

void test_photos_go_to_eyes_after_idle_and_take_buttons() {
  TEST_ASSERT_EQUAL(UiState::PHOTOS, uiStateAfterTime(UiState::PHOTOS, 0, 9999, false, TIMEOUTS));
  TEST_ASSERT_EQUAL(UiState::EYES, uiStateAfterTime(UiState::PHOTOS, 0, 10000, false, TIMEOUTS));
  TEST_ASSERT_EQUAL(UiButtonRoute::Screen, uiRouteButton(UiState::PHOTOS, tap(Button::BTN2_DOWN)));
}

void test_info_goes_to_eyes_after_idle_and_takes_buttons() {
  TEST_ASSERT_EQUAL(UiState::INFO, uiStateAfterTime(UiState::INFO, 0, 9999, false, TIMEOUTS));
  TEST_ASSERT_EQUAL(UiState::EYES, uiStateAfterTime(UiState::INFO, 0, 10000, false, TIMEOUTS));
  TEST_ASSERT_EQUAL(UiButtonRoute::Screen, uiRouteButton(UiState::INFO, tap(Button::BTN2_DOWN)));
}

void test_trash_and_multi_delete_behave_like_settings() {
  TEST_ASSERT_EQUAL(UiState::EYES, uiStateAfterTime(UiState::TRASH, 0, 10000, false, TIMEOUTS));
  TEST_ASSERT_EQUAL(UiState::MULTI_DELETE, uiStateAfterTime(UiState::MULTI_DELETE, 0, 9999, false, TIMEOUTS));
  TEST_ASSERT_EQUAL(UiButtonRoute::Screen, uiRouteButton(UiState::TRASH, tap(Button::BTN3_OK)));
  TEST_ASSERT_EQUAL(UiButtonRoute::Screen, uiRouteButton(UiState::MULTI_DELETE, tap(Button::BTN3_OK)));
}

void test_hold_is_done_after_hold_time_and_released_on_release() {
  TEST_ASSERT_EQUAL(HoldState::Waiting, holdStateAt(true, 1000, 2999, 2000));
  TEST_ASSERT_EQUAL(HoldState::Done, holdStateAt(true, 1000, 3000, 2000));
  TEST_ASSERT_EQUAL(HoldState::Released, holdStateAt(false, 1000, 5000, 2000));
  TEST_ASSERT_EQUAL(HoldState::Done, holdStateAt(true, 0xFFFFFF00UL, 0x00000800UL, 2000));
}

void test_menu_has_six_items_in_order() {
  TEST_ASSERT_EQUAL_UINT8(6, MENU_ITEM_COUNT);
  MenuModel m{};
  const MenuItem expected[] = {MenuItem::FAVORITES, MenuItem::PHOTOS, MenuItem::TRASH, MenuItem::SETTINGS, MenuItem::INFO,
                               MenuItem::MESSAGES};
  for (const MenuItem item : expected) {
    m = menuOnButton(m, tap(Button::BTN2_DOWN)).model;
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(item), m.selected);
  }
}

void test_menu_wraps_from_first_to_info_going_up() {
  const MenuStep up = menuOnButton(MenuModel{}, tap(Button::BTN1_UP));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(MenuItem::INFO), up.model.selected);
}

void test_eyes_turn_off_after_time_in_eyes() {
  TEST_ASSERT_EQUAL(UiState::EYES, uiStateAfterTime(UiState::EYES, 59999, 99999, false, TIMEOUTS));
  TEST_ASSERT_EQUAL(UiState::OFF, uiStateAfterTime(UiState::EYES, 60000, 99999, false, TIMEOUTS));
  TEST_ASSERT_EQUAL(UiState::OFF, uiStateAfterTime(UiState::OFF, 999999, 999999, false, TIMEOUTS));
}

void test_only_double_tap_wakes_from_eyes_and_off() {
  TEST_ASSERT_EQUAL(UiButtonRoute::Ignore, uiRouteButton(UiState::EYES, tap(Button::BTN3_OK)));
  TEST_ASSERT_EQUAL(UiButtonRoute::Ignore, uiRouteButton(UiState::OFF, longPress(Button::BTN3_OK)));
  TEST_ASSERT_EQUAL(UiButtonRoute::Wake, uiRouteButton(UiState::EYES, doubleTap(Button::BTN1_UP)));
  TEST_ASSERT_EQUAL(UiButtonRoute::Wake, uiRouteButton(UiState::OFF, doubleTap(Button::BTN4_BACK)));
}

void test_buttons_in_menu_go_to_screen_and_in_boot_count_as_input() {
  TEST_ASSERT_EQUAL(UiButtonRoute::Screen, uiRouteButton(UiState::MENU, tap(Button::BTN1_UP)));
  TEST_ASSERT_EQUAL(UiButtonRoute::Screen, uiRouteButton(UiState::MESSAGE_LIST, doubleTap(Button::BTN4_BACK)));
  TEST_ASSERT_EQUAL(UiButtonRoute::BootInput, uiRouteButton(UiState::BOOT, tap(Button::BTN2_DOWN)));
}

// ----- Menu -------------------------------------------------------------------

void test_menu_up_and_down_wrap_around() {
  const MenuModel top{};
  const MenuStep up = menuOnButton(top, tap(Button::BTN1_UP));
  TEST_ASSERT_EQUAL_UINT8(MENU_ITEM_COUNT - 1, up.model.selected);
  const MenuStep down = menuOnButton(up.model, tap(Button::BTN2_DOWN));
  TEST_ASSERT_EQUAL_UINT8(0, down.model.selected);
  TEST_ASSERT_EQUAL(MenuExit::None, down.exit);
}

void test_menu_does_not_modify_input_model() {
  const MenuModel top{};
  menuOnButton(top, tap(Button::BTN2_DOWN));
  TEST_ASSERT_EQUAL_UINT8(0, top.selected);
}

void test_menu_ok_opens_each_item() {
  const MenuModel onMessages{static_cast<uint8_t>(MenuItem::MESSAGES)};
  TEST_ASSERT_EQUAL(MenuExit::ToMessages, menuOnButton(onMessages, tap(Button::BTN3_OK)).exit);

  const MenuModel onFavorites{static_cast<uint8_t>(MenuItem::FAVORITES)};
  TEST_ASSERT_EQUAL(MenuExit::ToFavorites, menuOnButton(onFavorites, tap(Button::BTN3_OK)).exit);

  const MenuModel onPhotos{static_cast<uint8_t>(MenuItem::PHOTOS)};
  TEST_ASSERT_EQUAL(MenuExit::ToPhotos, menuOnButton(onPhotos, tap(Button::BTN3_OK)).exit);

  const MenuModel onTrash{static_cast<uint8_t>(MenuItem::TRASH)};
  TEST_ASSERT_EQUAL(MenuExit::ToTrash, menuOnButton(onTrash, tap(Button::BTN3_OK)).exit);

  const MenuModel onSettings{static_cast<uint8_t>(MenuItem::SETTINGS)};
  TEST_ASSERT_EQUAL(MenuExit::ToSettings, menuOnButton(onSettings, tap(Button::BTN3_OK)).exit);

  const MenuModel onInfo{static_cast<uint8_t>(MenuItem::INFO)};
  TEST_ASSERT_EQUAL(MenuExit::ToInfo, menuOnButton(onInfo, tap(Button::BTN3_OK)).exit);
}

void test_menu_back_exits_to_eyes_keeping_selection() {
  const MenuModel onInfo{static_cast<uint8_t>(MenuItem::INFO)};
  const MenuStep back = menuOnButton(onInfo, tap(Button::BTN4_BACK));
  TEST_ASSERT_EQUAL(MenuExit::ToEyes, back.exit);
  TEST_ASSERT_TRUE(back.model == onInfo);
}

void test_menu_long_press_does_nothing() {
  const MenuModel top{};
  const MenuStep step = menuOnButton(top, longPress(Button::BTN2_DOWN));
  TEST_ASSERT_TRUE(step.model == top);
  TEST_ASSERT_EQUAL(MenuExit::None, step.exit);
}

void test_menu_double_tap_counts_as_normal_press() {
  const MenuModel top{};
  TEST_ASSERT_EQUAL_UINT8(1, menuOnButton(top, doubleTap(Button::BTN2_DOWN)).model.selected);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_boot_goes_to_eyes_without_input_and_menu_with_input);
  RUN_TEST(test_menu_and_message_screens_go_to_eyes_after_idle);
  RUN_TEST(test_message_view_waits_longer_before_eyes);
  RUN_TEST(test_audio_view_waits_like_text_view_and_takes_buttons);
  RUN_TEST(test_settings_goes_to_eyes_after_menu_idle_and_takes_buttons);
  RUN_TEST(test_info_goes_to_eyes_after_idle_and_takes_buttons);
  RUN_TEST(test_trash_and_multi_delete_behave_like_settings);
  RUN_TEST(test_hold_is_done_after_hold_time_and_released_on_release);
  RUN_TEST(test_photos_go_to_eyes_after_idle_and_take_buttons);
  RUN_TEST(test_menu_has_six_items_in_order);
  RUN_TEST(test_menu_wraps_from_first_to_info_going_up);
  RUN_TEST(test_eyes_turn_off_after_time_in_eyes);
  RUN_TEST(test_only_double_tap_wakes_from_eyes_and_off);
  RUN_TEST(test_buttons_in_menu_go_to_screen_and_in_boot_count_as_input);
  RUN_TEST(test_menu_up_and_down_wrap_around);
  RUN_TEST(test_menu_does_not_modify_input_model);
  RUN_TEST(test_menu_ok_opens_each_item);
  RUN_TEST(test_menu_back_exits_to_eyes_keeping_selection);
  RUN_TEST(test_menu_long_press_does_nothing);
  RUN_TEST(test_menu_double_tap_counts_as_normal_press);
  return UNITY_END();
}
