#include <unity.h>

#include "audio_view_rules.h"

namespace {
ButtonEvent tap(Button b) {
  return ButtonEvent{b, false, false};
}

ButtonEvent longPress(Button b) {
  return ButtonEvent{b, false, true};
}

constexpr size_t OUT_SIZE = 24;
constexpr uint32_t BARS = 24;
constexpr int32_t MIN_H = 4;
constexpr int32_t MAX_H = 80;
}  // namespace

void setUp() {}
void tearDown() {}

// ----- Botones ------------------------------------------------------------------

void test_ok_plays_pauses_and_resumes() {
  TEST_ASSERT_EQUAL(AudioViewAction::Play, audioViewOnButton(PlayerState::Idle, tap(Button::BTN3_OK)));
  TEST_ASSERT_EQUAL(AudioViewAction::Pause, audioViewOnButton(PlayerState::Playing, tap(Button::BTN3_OK)));
  TEST_ASSERT_EQUAL(AudioViewAction::Resume, audioViewOnButton(PlayerState::Paused, tap(Button::BTN3_OK)));
}

void test_back_stops_while_playing_and_leaves_otherwise() {
  TEST_ASSERT_EQUAL(AudioViewAction::Stop, audioViewOnButton(PlayerState::Playing, tap(Button::BTN4_BACK)));
  TEST_ASSERT_EQUAL(AudioViewAction::Back, audioViewOnButton(PlayerState::Paused, tap(Button::BTN4_BACK)));
  TEST_ASSERT_EQUAL(AudioViewAction::Back, audioViewOnButton(PlayerState::Idle, tap(Button::BTN4_BACK)));
}

void test_volume_buttons_only_work_while_playing() {
  TEST_ASSERT_EQUAL(AudioViewAction::VolumeUp, audioViewOnButton(PlayerState::Playing, tap(Button::BTN1_UP)));
  TEST_ASSERT_EQUAL(AudioViewAction::VolumeDown, audioViewOnButton(PlayerState::Playing, tap(Button::BTN2_DOWN)));
  TEST_ASSERT_EQUAL(AudioViewAction::None, audioViewOnButton(PlayerState::Paused, tap(Button::BTN1_UP)));
  TEST_ASSERT_EQUAL(AudioViewAction::None, audioViewOnButton(PlayerState::Idle, tap(Button::BTN2_DOWN)));
}

void test_long_ok_toggles_favorite_and_other_long_presses_do_nothing() {
  TEST_ASSERT_EQUAL(AudioViewAction::ToggleFavorite,
                    audioViewOnButton(PlayerState::Playing, longPress(Button::BTN3_OK)));
  TEST_ASSERT_EQUAL(AudioViewAction::ToggleFavorite, audioViewOnButton(PlayerState::Idle, longPress(Button::BTN3_OK)));
  TEST_ASSERT_EQUAL(AudioViewAction::None, audioViewOnButton(PlayerState::Playing, longPress(Button::BTN4_BACK)));
}

// ----- Tiempo -------------------------------------------------------------------

void test_play_time_with_and_without_duration() {
  char out[OUT_SIZE];
  formatPlayTime(75, 200, out, sizeof(out));
  TEST_ASSERT_EQUAL_STRING("01:15 / 03:20", out);
  formatPlayTime(7, 0, out, sizeof(out));
  TEST_ASSERT_EQUAL_STRING("00:07", out);
}

void test_play_time_never_passes_duration() {
  char out[OUT_SIZE];
  formatPlayTime(31, 30, out, sizeof(out));
  TEST_ASSERT_EQUAL_STRING("00:30 / 00:30", out);
  formatPlayTime(0, 30, out, sizeof(out));
  TEST_ASSERT_EQUAL_STRING("00:00 / 00:30", out);
}

// ----- Ondas y volumen ------------------------------------------------------------

void test_wave_bars_stay_in_range_and_edges_are_lower() {
  for (uint32_t tick = 0; tick < 200; ++tick) {
    for (uint32_t bar = 0; bar < BARS; ++bar) {
      const int32_t h = waveBarHeight(bar, BARS, tick, MIN_H, MAX_H);
      TEST_ASSERT_TRUE(h >= MIN_H && h <= MAX_H);
    }
    // Los extremos no pasan del 35 % del recorrido
    TEST_ASSERT_TRUE(waveBarHeight(0, BARS, tick, MIN_H, MAX_H) <= MIN_H + (MAX_H - MIN_H) * 35 / 100);
  }
}

void test_wave_is_frozen_for_same_tick_and_moves_between_ticks() {
  TEST_ASSERT_EQUAL_INT32(waveBarHeight(12, BARS, 40, MIN_H, MAX_H), waveBarHeight(12, BARS, 40, MIN_H, MAX_H));
  int changed = 0;
  for (uint32_t bar = 0; bar < BARS; ++bar) {
    if (waveBarHeight(bar, BARS, 40, MIN_H, MAX_H) != waveBarHeight(bar, BARS, 41, MIN_H, MAX_H)) {
      ++changed;
    }
  }
  TEST_ASSERT_TRUE(changed > static_cast<int>(BARS) / 2);
}

void test_volume_fill_scales_and_clamps() {
  TEST_ASSERT_EQUAL_INT32(0, volumeFillWidth(0, 210));
  TEST_ASSERT_EQUAL_INT32(210, volumeFillWidth(AUDIO_VOLUME_MAX, 210));
  TEST_ASSERT_EQUAL_INT32(100, volumeFillWidth(10, 210));
  TEST_ASSERT_EQUAL_INT32(210, volumeFillWidth(200, 210));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_ok_plays_pauses_and_resumes);
  RUN_TEST(test_back_stops_while_playing_and_leaves_otherwise);
  RUN_TEST(test_volume_buttons_only_work_while_playing);
  RUN_TEST(test_long_ok_toggles_favorite_and_other_long_presses_do_nothing);
  RUN_TEST(test_play_time_with_and_without_duration);
  RUN_TEST(test_play_time_never_passes_duration);
  RUN_TEST(test_wave_bars_stay_in_range_and_edges_are_lower);
  RUN_TEST(test_wave_is_frozen_for_same_tick_and_moves_between_ticks);
  RUN_TEST(test_volume_fill_scales_and_clamps);
  return UNITY_END();
}
