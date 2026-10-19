#include <unity.h>

#include "settings_rules.h"

namespace {
ButtonEvent tap(Button b) {
  return ButtonEvent{b, false, false};
}

ButtonEvent longPress(Button b) {
  return ButtonEvent{b, false, true};
}

constexpr uint32_t MIN_SHOW_MS = 1000;
constexpr uint32_t TIMEOUT_MS = 10000;

SettingsModel inScreen(SettingsItem item) {
  const auto i = static_cast<uint8_t>(item);
  return {i, settingsScreenOf(i)};
}
}  // namespace

void setUp() {}
void tearDown() {}

// ----- Navegacion -----------------------------------------------------------------

void test_list_moves_with_wraparound_and_back_exits() {
  const SettingsModel top{};
  const SettingsStep up = settingsOnButton(top, tap(Button::BTN1_UP));
  TEST_ASSERT_EQUAL_UINT8(SETTINGS_ITEM_COUNT - 1, up.model.selected);
  TEST_ASSERT_EQUAL_UINT8(0, settingsOnButton(up.model, tap(Button::BTN2_DOWN)).model.selected);
  TEST_ASSERT_EQUAL(SettingsAction::Exit, settingsOnButton(top, tap(Button::BTN4_BACK)).action);
}

void test_settings_has_four_items_without_trash_and_wraps() {
  TEST_ASSERT_EQUAL_UINT8(4, SETTINGS_ITEM_COUNT);
  const SettingsModel last{static_cast<uint8_t>(SettingsItem::MULTI_DELETE), SettingsScreen::LIST};
  TEST_ASSERT_EQUAL_UINT8(0, settingsOnButton(last, tap(Button::BTN2_DOWN)).model.selected);
  const SettingsModel first{};
  TEST_ASSERT_EQUAL_UINT8(3, settingsOnButton(first, tap(Button::BTN1_UP)).model.selected);
}

void test_multi_delete_opens_its_own_state() {
  const SettingsModel onMulti{static_cast<uint8_t>(SettingsItem::MULTI_DELETE), SettingsScreen::LIST};
  const SettingsStep multi = settingsOnButton(onMulti, tap(Button::BTN3_OK));
  TEST_ASSERT_EQUAL(SettingsAction::OpenMultiDelete, multi.action);
  TEST_ASSERT_EQUAL(SettingsScreen::LIST, multi.model.screen);
}

void test_ok_opens_each_screen() {
  for (uint8_t i = 0; i <= static_cast<uint8_t>(SettingsItem::WIFI); ++i) {
    const SettingsModel onRow{i, SettingsScreen::LIST};
    const SettingsStep step = settingsOnButton(onRow, tap(Button::BTN3_OK));
    TEST_ASSERT_EQUAL(settingsScreenOf(i), step.model.screen);
    TEST_ASSERT_EQUAL(SettingsAction::None, step.action);
  }
  TEST_ASSERT_EQUAL(SettingsScreen::WIFI, settingsScreenOf(static_cast<uint8_t>(SettingsItem::WIFI)));
}

void test_back_from_screen_returns_to_same_row_and_long_press_does_nothing() {
  const SettingsModel wifi = inScreen(SettingsItem::WIFI);
  const SettingsStep back = settingsOnButton(wifi, tap(Button::BTN4_BACK));
  TEST_ASSERT_EQUAL(SettingsScreen::LIST, back.model.screen);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(SettingsItem::WIFI), back.model.selected);
  const SettingsStep held = settingsOnButton(wifi, longPress(Button::BTN4_BACK));
  TEST_ASSERT_TRUE(held.model == wifi);
  TEST_ASSERT_EQUAL(SettingsAction::None, held.action);
}

void test_screen_buttons_give_their_actions() {
  const SettingsModel volume = inScreen(SettingsItem::VOLUME);
  TEST_ASSERT_EQUAL(SettingsAction::VolumeUp, settingsOnButton(volume, tap(Button::BTN1_UP)).action);
  TEST_ASSERT_EQUAL(SettingsAction::VolumeDown, settingsOnButton(volume, tap(Button::BTN2_DOWN)).action);
  TEST_ASSERT_EQUAL(SettingsAction::None, settingsOnButton(volume, tap(Button::BTN3_OK)).action);
  const SettingsModel sound = inScreen(SettingsItem::NOTIFICATION_SOUND);
  TEST_ASSERT_EQUAL(SettingsAction::ToggleSound, settingsOnButton(sound, tap(Button::BTN3_OK)).action);
  TEST_ASSERT_EQUAL(SettingsAction::None, settingsOnButton(sound, tap(Button::BTN1_UP)).action);
  const SettingsModel wifi = inScreen(SettingsItem::WIFI);
  TEST_ASSERT_EQUAL(SettingsAction::ReconnectWifi, settingsOnButton(wifi, tap(Button::BTN3_OK)).action);
}

// ----- Valores ------------------------------------------------------------------

void test_volume_changes_by_step_within_limits() {
  TEST_ASSERT_EQUAL_UINT8(16, settingsVolumeAfter(15, true, 1));
  TEST_ASSERT_EQUAL_UINT8(14, settingsVolumeAfter(15, false, 1));
  TEST_ASSERT_EQUAL_UINT8(AUDIO_VOLUME_MAX, settingsVolumeAfter(AUDIO_VOLUME_MAX, true, 1));
  TEST_ASSERT_EQUAL_UINT8(0, settingsVolumeAfter(0, false, 1));
  TEST_ASSERT_EQUAL_UINT8(AUDIO_VOLUME_MAX, settingsVolumeAfter(19, true, 5));
  TEST_ASSERT_EQUAL_UINT8(0, settingsVolumeAfter(2, false, 5));
}

void test_sound_toggles_and_labels() {
  TEST_ASSERT_FALSE(settingsToggledSound(true));
  TEST_ASSERT_TRUE(settingsToggledSound(false));
  TEST_ASSERT_EQUAL_STRING("ON", settingsOnOffLabel(true));
  TEST_ASSERT_EQUAL_STRING("OFF", settingsOnOffLabel(false));
}

// ----- WiFi -----------------------------------------------------------------------

void test_wifi_status_shows_connecting_until_connected_or_timeout() {
  TEST_ASSERT_EQUAL(WifiScreenStatus::Connected, wifiScreenStatus(true, false, 0, MIN_SHOW_MS, TIMEOUT_MS));
  TEST_ASSERT_EQUAL(WifiScreenStatus::Disconnected, wifiScreenStatus(false, false, 0, MIN_SHOW_MS, TIMEOUT_MS));
  TEST_ASSERT_EQUAL(WifiScreenStatus::Connecting, wifiScreenStatus(false, true, 9999, MIN_SHOW_MS, TIMEOUT_MS));
  TEST_ASSERT_EQUAL(WifiScreenStatus::Disconnected, wifiScreenStatus(false, true, 10000, MIN_SHOW_MS, TIMEOUT_MS));
  // Aunque ya hubiera conexion, "Conectando..." se ve al menos minShowMs
  TEST_ASSERT_EQUAL(WifiScreenStatus::Connecting, wifiScreenStatus(true, true, 500, MIN_SHOW_MS, TIMEOUT_MS));
  TEST_ASSERT_EQUAL(WifiScreenStatus::Connected, wifiScreenStatus(true, true, 1000, MIN_SHOW_MS, TIMEOUT_MS));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_list_moves_with_wraparound_and_back_exits);
  RUN_TEST(test_settings_has_four_items_without_trash_and_wraps);
  RUN_TEST(test_multi_delete_opens_its_own_state);
  RUN_TEST(test_ok_opens_each_screen);
  RUN_TEST(test_back_from_screen_returns_to_same_row_and_long_press_does_nothing);
  RUN_TEST(test_screen_buttons_give_their_actions);
  RUN_TEST(test_volume_changes_by_step_within_limits);
  RUN_TEST(test_sound_toggles_and_labels);
  RUN_TEST(test_wifi_status_shows_connecting_until_connected_or_timeout);
  return UNITY_END();
}
