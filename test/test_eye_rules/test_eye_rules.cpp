#include <unity.h>

#include <cstdlib>
#include <cstring>

#include "eye_shapes.h"

namespace {
constexpr uint32_t SEEDS = 40;
constexpr uint32_t FRAME_MS = 1000 / 30;

bool isMicroMood(EyeMood mood) {
  return mood == EyeMood::HAPPY || mood == EyeMood::SURPRISED || mood == EyeMood::WINKING ||
         mood == EyeMood::LAUGHING || mood == EyeMood::IN_LOVE;
}

EyeFrame frameOf(EyeMood mood, uint32_t msInMood, uint32_t variant = 0, uint32_t msInEyes = 0, uint32_t seed = 7) {
  return eyeFrameAt(EyeScene{mood, msInMood, variant}, msInEyes, seed);
}

bool isOpenRound(const EyeShape& s) {
  return s.form == EyeForm::OPEN && s.pupil == PupilForm::ROUND;
}
}  // namespace

void setUp() {}
void tearDown() {}

// ----- Programa por tiempo ----------------------------------------------------------

void test_every_mood_has_its_own_name() {
  for (uint8_t i = 0; i < EYE_MOOD_COUNT; ++i) {
    const char* name = eyeMoodName(static_cast<EyeMood>(i));
    TEST_ASSERT_NOT_EQUAL(0, strcmp(name, "?"));
    for (uint8_t j = 0; j < i; ++j) {
      TEST_ASSERT_NOT_EQUAL(0, strcmp(name, eyeMoodName(static_cast<EyeMood>(j))));
    }
  }
}

void test_base_mood_follows_the_phases() {
  for (uint32_t seed = 0; seed < SEEDS; ++seed) {
    for (uint32_t ms = 0; ms < 60000; ms += 250) {
      const EyeMood mood = eyeBaseScene(ms, seed).mood;
      if (ms < 15000) {
        TEST_ASSERT_TRUE(mood == EyeMood::LOOKING || mood == EyeMood::BLINKING);
      } else if (ms < 30000) {
        TEST_ASSERT_TRUE(mood == EyeMood::LOOKING_AROUND || mood == EyeMood::THINKING);
      } else if (ms < 45000) {
        TEST_ASSERT_TRUE(mood == EyeMood::TIRED || mood == EyeMood::CONFUSED);
      } else {
        TEST_ASSERT_EQUAL(EyeMood::SLEEPING, mood);
      }
    }
  }
}

void test_each_phase_alternates_both_moods() {
  const uint32_t seed = 3;
  TEST_ASSERT_NOT_EQUAL(eyeBaseScene(1000, seed).mood, eyeBaseScene(8000, seed).mood);
  TEST_ASSERT_NOT_EQUAL(eyeBaseScene(16000, seed).mood, eyeBaseScene(23000, seed).mood);
  TEST_ASSERT_NOT_EQUAL(eyeBaseScene(31000, seed).mood, eyeBaseScene(38000, seed).mood);
  TEST_ASSERT_EQUAL_UINT32(500, eyeBaseScene(8000, seed).msInMood);
}

void test_micro_expressions_every_10_to_20_s_with_their_duration() {
  bool seen[EYE_MOOD_COUNT] = {};
  for (uint32_t seed = 0; seed < SEEDS; ++seed) {
    uint32_t lastStart = 0;
    bool hasLast = false;
    EyeMood previous = EyeMood::LOOKING;
    uint32_t startMs = 0;
    for (uint32_t ms = 0; ms <= 60000; ms += 10) {
      const EyeMood mood = eyeSceneAt(ms, false, seed).mood;
      const bool wasMicro = isMicroMood(previous);
      if (isMicroMood(mood) && !wasMicro) {
        TEST_ASSERT_LESS_THAN_UINT32(45000, ms);
        if (hasLast) {
          TEST_ASSERT_UINT32_WITHIN(5010, 15000, ms - lastStart);
        }
        lastStart = ms;
        hasLast = true;
        startMs = ms;
        seen[static_cast<uint8_t>(mood)] = true;
      }
      if (!isMicroMood(mood) && wasMicro) {
        TEST_ASSERT_UINT32_WITHIN(10, eyeMoodDurationMs(previous), ms - startMs);
      }
      previous = mood;
    }
  }
  TEST_ASSERT_TRUE(seen[static_cast<uint8_t>(EyeMood::HAPPY)]);
  TEST_ASSERT_TRUE(seen[static_cast<uint8_t>(EyeMood::SURPRISED)]);
  TEST_ASSERT_TRUE(seen[static_cast<uint8_t>(EyeMood::WINKING)]);
  TEST_ASSERT_TRUE(seen[static_cast<uint8_t>(EyeMood::LAUGHING)]);
  TEST_ASSERT_TRUE(seen[static_cast<uint8_t>(EyeMood::IN_LOVE)]);
}

void test_wink_and_laugh_last_about_1_and_1_5_s() {
  TEST_ASSERT_EQUAL_UINT32(1000, eyeMoodDurationMs(EyeMood::WINKING));
  TEST_ASSERT_EQUAL_UINT32(1500, eyeMoodDurationMs(EyeMood::LAUGHING));
  TEST_ASSERT_EQUAL_UINT32(1000, eyeMoodDurationMs(EyeMood::HAPPY));
}

void test_message_surprise_overrides_everything() {
  TEST_ASSERT_EQUAL(EyeMood::SURPRISED, eyeSceneAt(0, true, 1).mood);
  TEST_ASSERT_EQUAL(EyeMood::SURPRISED, eyeSceneAt(50000, true, 1).mood);
}

void test_boot_is_happy() {
  TEST_ASSERT_EQUAL(EyeMood::HAPPY, eyeBootScene(500, 9).mood);
}

// ----- Parpadeo ---------------------------------------------------------------------

void test_blink_closes_linearly_then_holds_closed_then_opens() {
  TEST_ASSERT_EQUAL_UINT8(100, eyeBlinkOpenPct(0, 120, 40));
  TEST_ASSERT_EQUAL_UINT8(50, eyeBlinkOpenPct(20, 120, 40));
  TEST_ASSERT_EQUAL_UINT8(0, eyeBlinkOpenPct(40, 120, 40));
  TEST_ASSERT_EQUAL_UINT8(0, eyeBlinkOpenPct(79, 120, 40));
  TEST_ASSERT_EQUAL_UINT8(50, eyeBlinkOpenPct(100, 120, 40));
  TEST_ASSERT_EQUAL_UINT8(100, eyeBlinkOpenPct(120, 120, 40));
  TEST_ASSERT_EQUAL_UINT8(100, eyeBlinkOpenPct(5000, 120, 40));
}

void test_blink_is_seen_closed_at_30_fps_whatever_the_phase() {
  for (uint32_t phase = 0; phase < FRAME_MS; ++phase) {
    bool wasClosed = false;
    for (uint32_t t = phase; t < EYE_BLINK_MS; t += FRAME_MS) {
      wasClosed = wasClosed || eyeBlinkOpenPct(t, EYE_BLINK_MS, EYE_BLINK_CLOSED_MS) == 0;
    }
    TEST_ASSERT_TRUE_MESSAGE(wasClosed, "algun desfase se salta el ojo cerrado");
  }
}

void test_blink_without_closing_time_does_not_divide_by_zero() {
  TEST_ASSERT_EQUAL_UINT8(0, eyeBlinkOpenPct(0, 40, 40));
  TEST_ASSERT_EQUAL_UINT8(100, eyeBlinkOpenPct(40, 40, 40));
}

// Inicios de parpadeo (el segundo de un doble no cuenta como nuevo)
struct BlinkStats {
  uint32_t minGapMs;
  uint32_t maxGapMs;
  uint32_t doubles;
  uint32_t blinks;
};

BlinkStats blinkStats(EyeBlinkStyle style) {
  BlinkStats stats{UINT32_MAX, 0, 0, 0};
  for (uint32_t seed = 0; seed < SEEDS; ++seed) {
    bool wasOpen = true;
    bool hasLast = false;
    uint32_t lastStart = 0;
    for (uint32_t ms = 0; ms < 60000; ++ms) {
      const bool isOpen = eyeBlinkAt(ms, seed, style) == 100;
      if (wasOpen && !isOpen) {
        if (hasLast && ms - lastStart < 1000) {
          ++stats.doubles;
        } else {
          if (hasLast) {
            stats.minGapMs = ms - lastStart < stats.minGapMs ? ms - lastStart : stats.minGapMs;
            stats.maxGapMs = ms - lastStart > stats.maxGapMs ? ms - lastStart : stats.maxGapMs;
          }
          lastStart = ms;
          hasLast = true;
          ++stats.blinks;
        }
      }
      wasOpen = isOpen;
    }
  }
  return stats;
}

void test_blinks_every_3_to_6_s_and_some_are_double() {
  const BlinkStats s = blinkStats(EyeBlinkStyle::NORMAL);
  TEST_ASSERT_GREATER_OR_EQUAL_UINT32(3000, s.minGapMs);
  TEST_ASSERT_LESS_OR_EQUAL_UINT32(6000, s.maxGapMs);
  TEST_ASSERT_GREATER_THAN_UINT32(0, s.doubles);
  TEST_ASSERT_LESS_THAN_UINT32(s.blinks, s.doubles);
}

void test_blinking_mood_always_blinks_twice() {
  const BlinkStats s = blinkStats(EyeBlinkStyle::ALWAYS_DOUBLE);
  TEST_ASSERT_UINT32_WITHIN(SEEDS, s.blinks, s.doubles);
}

void test_thinking_blinks_less() {
  const BlinkStats s = blinkStats(EyeBlinkStyle::REDUCED);
  TEST_ASSERT_GREATER_OR_EQUAL_UINT32(6000, s.minGapMs);
}

// ----- Forma de cada cara -------------------------------------------------------------

void test_looking_is_open_round_with_small_moves() {
  bool hasMoved = false;
  for (uint32_t ms = 0; ms < 15000; ms += FRAME_MS) {
    const EyeFrame f = frameOf(EyeMood::LOOKING, ms, 0, ms);
    TEST_ASSERT_TRUE(isOpenRound(f.left) && isOpenRound(f.right));
    TEST_ASSERT_TRUE(std::abs(f.left.pupilDx) <= EYE_SACCADE_RADIUS && std::abs(f.left.pupilDy) <= EYE_SACCADE_RADIUS);
    hasMoved = hasMoved || f.left.pupilDx != 0 || f.left.pupilDy != 0;
  }
  TEST_ASSERT_TRUE(hasMoved);
}

void test_blinking_keeps_pupil_centered() {
  const EyeFrame f = frameOf(EyeMood::BLINKING, 2000, 0, 2000);
  TEST_ASSERT_TRUE(isOpenRound(f.left));
  TEST_ASSERT_EQUAL_INT16(0, f.left.pupilDx);
  TEST_ASSERT_EQUAL_INT16(0, f.left.pupilDy);
}

void test_looking_around_takes_different_paths() {
  bool isDifferent = false;
  for (uint32_t ms = 0; ms < 7500; ms += 100) {
    const EyeFrame a = frameOf(EyeMood::LOOKING_AROUND, ms, 1);
    const EyeFrame b = frameOf(EyeMood::LOOKING_AROUND, ms, 2);
    TEST_ASSERT_TRUE(std::abs(a.left.pupilDx) <= EYE_ORBIT_RADIUS && std::abs(a.left.pupilDy) <= EYE_ORBIT_RADIUS);
    isDifferent = isDifferent || !(a == b);
  }
  TEST_ASSERT_TRUE(isDifferent);
}

void test_happy_is_two_arcs_with_varying_curve() {
  const EyeFrame a = frameOf(EyeMood::HAPPY, 0, 0);
  const EyeFrame b = frameOf(EyeMood::HAPPY, 0, 17);
  TEST_ASSERT_EQUAL(EyeForm::ARC, a.left.form);
  TEST_ASSERT_EQUAL(EyeForm::ARC, a.right.form);
  TEST_ASSERT_TRUE(a.left.arcPct >= EYE_HAPPY_MIN_ARC_PCT && a.left.arcPct <= EYE_HAPPY_MAX_ARC_PCT);
  TEST_ASSERT_NOT_EQUAL(a.left.arcPct, b.left.arcPct);
}

void test_surprised_is_big_with_small_trembling_pupil() {
  const EyeFrame f = frameOf(EyeMood::SURPRISED, 0, 0, 0);
  TEST_ASSERT_EQUAL_UINT8(EYE_SURPRISE_SCALE_PCT, f.left.scalePct);
  TEST_ASSERT_EQUAL_UINT8(EYE_SURPRISE_PUPIL_PCT, f.left.pupilPct);
  bool hasTrembled = false;
  for (uint32_t ms = 0; ms < 1000; ms += FRAME_MS) {
    const EyeFrame g = frameOf(EyeMood::SURPRISED, ms, 0, ms);
    TEST_ASSERT_TRUE(std::abs(g.left.pupilDx) <= EYE_SURPRISE_JITTER_PX);
    hasTrembled = hasTrembled || !(g == f);
  }
  TEST_ASSERT_TRUE(hasTrembled);
}

void test_sleeping_is_a_line_that_breathes() {
  const EyeFrame still = frameOf(EyeMood::SLEEPING, 0);
  const EyeFrame up = frameOf(EyeMood::SLEEPING, EYE_SLEEP_BREATH_MS / 4);
  TEST_ASSERT_EQUAL(EyeForm::LINE, still.left.form);
  TEST_ASSERT_EQUAL(EyeForm::LINE, still.right.form);
  TEST_ASSERT_EQUAL_INT16(0, still.left.dy);
  TEST_ASSERT_EQUAL_INT16(EYE_SLEEP_BREATH_PX, up.left.dy);
}

void test_tired_lid_drops_slowly_and_rises_again() {
  const EyeFrame normal = frameOf(EyeMood::TIRED, 0);
  const EyeFrame droop = frameOf(EyeMood::TIRED, EYE_TIRED_DROOP_FROM_MS + EYE_TIRED_DROOP_MS / 2);
  const EyeFrame again = frameOf(EyeMood::TIRED, EYE_TIRED_CYCLE_MS);
  TEST_ASSERT_EQUAL_UINT8(EYE_TIRED_LID_PCT, normal.left.lidPct);
  TEST_ASSERT_EQUAL_UINT8(EYE_TIRED_DROOP_LID_PCT, droop.left.lidPct);
  TEST_ASSERT_EQUAL_UINT8(EYE_TIRED_LID_PCT, again.left.lidPct);
  TEST_ASSERT_GREATER_THAN_INT16(0, normal.left.pupilDy);
}

void test_confused_has_one_big_eye_that_swaps() {
  const EyeFrame a = frameOf(EyeMood::CONFUSED, 0);
  const EyeFrame b = frameOf(EyeMood::CONFUSED, EYE_CONFUSED_SWAP_MS);
  TEST_ASSERT_NOT_EQUAL(a.left.scalePct, a.right.scalePct);
  TEST_ASSERT_EQUAL_UINT8(a.left.scalePct, b.right.scalePct);
  TEST_ASSERT_LESS_THAN_INT16(0, a.left.pupilDy);
}

void test_thinking_looks_up_then_aside_then_back() {
  const EyeFrame up = frameOf(EyeMood::THINKING, EYE_THINK_STEP_MS, 0);
  const EyeFrame aside = frameOf(EyeMood::THINKING, 2 * EYE_THINK_STEP_MS, 0);
  const EyeFrame back = frameOf(EyeMood::THINKING, 4 * EYE_THINK_STEP_MS, 0);
  TEST_ASSERT_EQUAL_INT16(-EYE_THINK_UP_PX, up.left.pupilDy);
  TEST_ASSERT_EQUAL_INT16(0, up.left.pupilDx);
  TEST_ASSERT_NOT_EQUAL(0, aside.left.pupilDx);
  TEST_ASSERT_EQUAL_INT16(0, back.left.pupilDx);
  TEST_ASSERT_EQUAL_INT16(0, back.left.pupilDy);
}

void test_winking_closes_one_eye_that_blinks_before_opening() {
  const EyeFrame start = frameOf(EyeMood::WINKING, 0, 0);
  TEST_ASSERT_NOT_EQUAL(start.left.form, start.right.form);
  const EyeShape& winker = start.left.form == EyeForm::ARC ? start.left : start.right;
  TEST_ASSERT_EQUAL(EyeForm::ARC, winker.form);
  bool wasClosedAgain = false;
  for (uint32_t ms = EYE_WINK_CLOSED_MS + EYE_WINK_OPENING_MS; ms < EYE_WINK_MS; ++ms) {
    wasClosedAgain = wasClosedAgain || frameOf(EyeMood::WINKING, ms, 0).left.openPct == 0;
  }
  TEST_ASSERT_TRUE(wasClosedAgain);
  const EyeFrame end = frameOf(EyeMood::WINKING, EYE_WINK_MS - 1, 0);
  TEST_ASSERT_TRUE(isOpenRound(end.left) && isOpenRound(end.right));
  TEST_ASSERT_EQUAL_UINT8(100, end.left.openPct);
}

void test_laughing_arcs_shake_and_change_curve() {
  bool hasShaken = false;
  uint8_t minArc = 255;
  uint8_t maxArc = 0;
  for (uint32_t ms = 0; ms < EYE_LAUGH_MS; ms += 10) {
    const EyeFrame f = frameOf(EyeMood::LAUGHING, ms);
    TEST_ASSERT_EQUAL(EyeForm::ARC, f.left.form);
    TEST_ASSERT_TRUE(std::abs(f.left.dy) <= EYE_LAUGH_SHAKE_PX);
    TEST_ASSERT_TRUE(f.left.arcPct <= EYE_MAX_ARC_PCT);
    hasShaken = hasShaken || f.left.dy != 0;
    minArc = f.left.arcPct < minArc ? f.left.arcPct : minArc;
    maxArc = f.left.arcPct > maxArc ? f.left.arcPct : maxArc;
  }
  TEST_ASSERT_TRUE(hasShaken);
  TEST_ASSERT_GREATER_THAN_UINT8(minArc + 20, maxArc);
}

void test_in_love_has_beating_heart_pupils() {
  const EyeFrame rest = frameOf(EyeMood::IN_LOVE, 0);
  const EyeFrame beat = frameOf(EyeMood::IN_LOVE, EYE_HEART_BEAT_MS / 4);
  TEST_ASSERT_EQUAL(PupilForm::HEART, rest.left.pupil);
  TEST_ASSERT_EQUAL(PupilForm::HEART, rest.right.pupil);
  TEST_ASSERT_EQUAL_UINT8(100, rest.left.pupilPct);
  TEST_ASSERT_EQUAL_UINT8(100 + EYE_HEART_BEAT_PCT, beat.left.pupilPct);
}

void test_same_time_and_seed_give_same_frame() {
  for (uint32_t ms = 0; ms < 60000; ms += 777) {
    const EyeScene a = eyeSceneAt(ms, false, 42);
    const EyeScene b = eyeSceneAt(ms, false, 42);
    TEST_ASSERT_EQUAL(a.mood, b.mood);
    TEST_ASSERT_TRUE(eyeFrameAt(a, ms, 42) == eyeFrameAt(b, ms, 42));
  }
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_every_mood_has_its_own_name);
  RUN_TEST(test_base_mood_follows_the_phases);
  RUN_TEST(test_each_phase_alternates_both_moods);
  RUN_TEST(test_micro_expressions_every_10_to_20_s_with_their_duration);
  RUN_TEST(test_wink_and_laugh_last_about_1_and_1_5_s);
  RUN_TEST(test_message_surprise_overrides_everything);
  RUN_TEST(test_boot_is_happy);
  RUN_TEST(test_blink_closes_linearly_then_holds_closed_then_opens);
  RUN_TEST(test_blink_is_seen_closed_at_30_fps_whatever_the_phase);
  RUN_TEST(test_blink_without_closing_time_does_not_divide_by_zero);
  RUN_TEST(test_blinks_every_3_to_6_s_and_some_are_double);
  RUN_TEST(test_blinking_mood_always_blinks_twice);
  RUN_TEST(test_thinking_blinks_less);
  RUN_TEST(test_looking_is_open_round_with_small_moves);
  RUN_TEST(test_blinking_keeps_pupil_centered);
  RUN_TEST(test_looking_around_takes_different_paths);
  RUN_TEST(test_happy_is_two_arcs_with_varying_curve);
  RUN_TEST(test_surprised_is_big_with_small_trembling_pupil);
  RUN_TEST(test_sleeping_is_a_line_that_breathes);
  RUN_TEST(test_tired_lid_drops_slowly_and_rises_again);
  RUN_TEST(test_confused_has_one_big_eye_that_swaps);
  RUN_TEST(test_thinking_looks_up_then_aside_then_back);
  RUN_TEST(test_winking_closes_one_eye_that_blinks_before_opening);
  RUN_TEST(test_laughing_arcs_shake_and_change_curve);
  RUN_TEST(test_in_love_has_beating_heart_pupils);
  RUN_TEST(test_same_time_and_seed_give_same_frame);
  return UNITY_END();
}
