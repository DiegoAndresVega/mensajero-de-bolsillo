#include <unity.h>

#include "audio_rules.h"

void setUp() {}
void tearDown() {}

void test_play_always_starts_and_replaces_current() {
  TEST_ASSERT_EQUAL(AudioAction::Start, decideAudioAction(AUDIO_CMD_PLAY, AudioSource::None));
  TEST_ASSERT_EQUAL(AudioAction::Start, decideAudioAction(AUDIO_CMD_PLAY, AudioSource::Message));
  TEST_ASSERT_EQUAL(AudioAction::Start, decideAudioAction(AUDIO_CMD_PLAY, AudioSource::Notification));
}

void test_notify_interrupts_a_message_but_not_another_notification() {
  TEST_ASSERT_EQUAL(AudioAction::Start, decideAudioAction(AUDIO_CMD_NOTIFY, AudioSource::None));
  TEST_ASSERT_EQUAL(AudioAction::Interrupt, decideAudioAction(AUDIO_CMD_NOTIFY, AudioSource::Message));
  TEST_ASSERT_EQUAL(AudioAction::Ignore, decideAudioAction(AUDIO_CMD_NOTIFY, AudioSource::Notification));
  // Con un mensaje en pausa no suena nada: la notificacion suena
  TEST_ASSERT_EQUAL(AudioAction::Start, decideAudioAction(AUDIO_CMD_NOTIFY, AudioSource::None, true));
}

void test_pause_only_acts_on_a_message_and_resume_needs_a_paused_one() {
  TEST_ASSERT_EQUAL(AudioAction::Pause, decideAudioAction(AUDIO_CMD_PAUSE, AudioSource::Message));
  TEST_ASSERT_EQUAL(AudioAction::Ignore, decideAudioAction(AUDIO_CMD_PAUSE, AudioSource::Notification));
  TEST_ASSERT_EQUAL(AudioAction::Ignore, decideAudioAction(AUDIO_CMD_PAUSE, AudioSource::None));
  TEST_ASSERT_EQUAL(AudioAction::Resume, decideAudioAction(AUDIO_CMD_RESUME, AudioSource::Notification, true));
  TEST_ASSERT_EQUAL(AudioAction::Ignore, decideAudioAction(AUDIO_CMD_RESUME, AudioSource::None, false));
}

void test_stop_notify_only_cuts_the_notification() {
  TEST_ASSERT_EQUAL(AudioAction::StopSound, decideAudioAction(AUDIO_CMD_STOP_NOTIFY, AudioSource::Notification));
  TEST_ASSERT_EQUAL(AudioAction::Ignore, decideAudioAction(AUDIO_CMD_STOP_NOTIFY, AudioSource::Message));
  TEST_ASSERT_EQUAL(AudioAction::Ignore, decideAudioAction(AUDIO_CMD_STOP_NOTIFY, AudioSource::None, true));
}

void test_player_state_follows_source_and_pause() {
  TEST_ASSERT_EQUAL(PlayerState::Playing, playerStateOf(AudioSource::Message, false));
  TEST_ASSERT_EQUAL(PlayerState::Paused, playerStateOf(AudioSource::Notification, true));
  TEST_ASSERT_EQUAL(PlayerState::Paused, playerStateOf(AudioSource::None, true));
  TEST_ASSERT_EQUAL(PlayerState::Idle, playerStateOf(AudioSource::Notification, false));
}

void test_played_ms_adds_running_segment_and_survives_wraparound() {
  TEST_ASSERT_EQUAL_UINT32(3000, playedMs(3000, 5000, 9000, false));
  TEST_ASSERT_EQUAL_UINT32(7000, playedMs(3000, 5000, 9000, true));
  TEST_ASSERT_EQUAL_UINT32(1256, playedMs(0, 0xFFFFFF00UL, 0x000003E8UL, true));
}

void test_stop_only_acts_when_playing_or_paused() {
  TEST_ASSERT_EQUAL(AudioAction::Ignore, decideAudioAction(AUDIO_CMD_STOP, AudioSource::None));
  TEST_ASSERT_EQUAL(AudioAction::Stop, decideAudioAction(AUDIO_CMD_STOP, AudioSource::None, true));
  TEST_ASSERT_EQUAL(AudioAction::Stop, decideAudioAction(AUDIO_CMD_STOP, AudioSource::Message));
  TEST_ASSERT_EQUAL(AudioAction::Stop, decideAudioAction(AUDIO_CMD_STOP, AudioSource::Notification));
}

void test_volume_is_clamped_to_range() {
  TEST_ASSERT_EQUAL_UINT8(0, clampAudioVolume(-5));
  TEST_ASSERT_EQUAL_UINT8(0, clampAudioVolume(0));
  TEST_ASSERT_EQUAL_UINT8(15, clampAudioVolume(15));
  TEST_ASSERT_EQUAL_UINT8(AUDIO_VOLUME_MAX, clampAudioVolume(AUDIO_VOLUME_MAX));
  TEST_ASSERT_EQUAL_UINT8(AUDIO_VOLUME_MAX, clampAudioVolume(300));
}

void test_valid_audio_paths_are_accepted() {
  TEST_ASSERT_TRUE(isValidAudioPath("/audios/abc123.mp3"));
  TEST_ASSERT_TRUE(isValidAudioPath("/sonidos/notificacion.mp3"));
}

void test_invalid_audio_paths_are_rejected() {
  TEST_ASSERT_FALSE(isValidAudioPath(nullptr));
  TEST_ASSERT_FALSE(isValidAudioPath(""));
  TEST_ASSERT_FALSE(isValidAudioPath("audios/sin_barra.mp3"));
  TEST_ASSERT_FALSE(isValidAudioPath("/audios/../config.json"));
  // 64 caracteres: no cabe con el terminador en AudioCmd::path
  TEST_ASSERT_FALSE(isValidAudioPath("/audios/0123456789012345678901234567890123456789012345678901.mp3"));
}

void test_elapsed_survives_millis_wraparound() {
  TEST_ASSERT_FALSE(hasElapsed(1000, 1500, 1000));
  TEST_ASSERT_TRUE(hasElapsed(1000, 2000, 1000));
  // millis() da la vuelta a los ~49 dias
  TEST_ASSERT_TRUE(hasElapsed(0xFFFFFF00UL, 0x00000400UL, 1000));
  TEST_ASSERT_FALSE(hasElapsed(0xFFFFFF00UL, 0x00000100UL, 1000));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_play_always_starts_and_replaces_current);
  RUN_TEST(test_notify_interrupts_a_message_but_not_another_notification);
  RUN_TEST(test_pause_only_acts_on_a_message_and_resume_needs_a_paused_one);
  RUN_TEST(test_stop_notify_only_cuts_the_notification);
  RUN_TEST(test_player_state_follows_source_and_pause);
  RUN_TEST(test_played_ms_adds_running_segment_and_survives_wraparound);
  RUN_TEST(test_stop_only_acts_when_playing_or_paused);
  RUN_TEST(test_volume_is_clamped_to_range);
  RUN_TEST(test_valid_audio_paths_are_accepted);
  RUN_TEST(test_invalid_audio_paths_are_rejected);
  RUN_TEST(test_elapsed_survives_millis_wraparound);
  return UNITY_END();
}
