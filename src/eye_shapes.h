#pragma once

// Forma de cada ojo en cada cara, sin dependencias de Arduino (tests en el
// entorno native). eyes.cpp solo usa eyeFrameAt; el resto son las piezas de
// cada cara. Tiempos y amplitudes en eye_rules.h.

#include "eye_rules.h"

struct EyeOffset {
  int16_t dx;
  int16_t dy;
};

constexpr float EYE_TWO_PI = 6.2831853F;
constexpr uint8_t EYE_FULL_PCT = 100;

// ----- Piezas ---------------------------------------------------------------------

// Seno de periodo periodMs y amplitud `amplitude`
inline int16_t eyeWave(uint32_t ms, uint32_t periodMs, int16_t amplitude) {
  const float phase = EYE_TWO_PI * static_cast<float>(ms % periodMs) / static_cast<float>(periodMs);
  return static_cast<int16_t>(std::lround(amplitude * std::sin(phase)));
}

// De `from` a `to` en durationMs (lineal)
inline EyeOffset eyeLerp(EyeOffset from, EyeOffset to, uint32_t t, uint32_t durationMs) {
  if (t >= durationMs) {
    return to;
  }
  const int32_t num = static_cast<int32_t>(t);
  const int32_t den = static_cast<int32_t>(durationMs);
  return {static_cast<int16_t>(from.dx + (to.dx - from.dx) * num / den),
          static_cast<int16_t>(from.dy + (to.dy - from.dy) * num / den)};
}

// Punto al azar en el cuadrado [-radius, radius]
inline EyeOffset eyeRandomOffset(uint32_t h, int16_t radius) {
  const uint32_t span = 2U * static_cast<uint32_t>(radius) + 1U;
  return {static_cast<int16_t>(static_cast<int32_t>(h % span) - radius),
          static_cast<int16_t>(static_cast<int32_t>((h >> 16) % span) - radius)};
}

inline EyeOffset eyePolar(float angle, int16_t radius) {
  return {static_cast<int16_t>(std::lround(radius * std::cos(angle))),
          static_cast<int16_t>(std::lround(radius * std::sin(angle)))};
}

inline EyeShape eyeOpen(uint8_t openPct, EyeOffset pupil) {
  EyeShape s;
  s.openPct = openPct;
  s.pupilDx = pupil.dx;
  s.pupilDy = pupil.dy;
  return s;
}

inline EyeShape eyeArc(uint8_t arcPct, int16_t dy) {
  EyeShape s;
  s.form = EyeForm::ARC;
  s.arcPct = arcPct;
  s.dy = dy;
  return s;
}

inline EyeFrame eyeBoth(const EyeShape& s) {
  return {s, s};
}

// ----- Recorridos de la pupila ----------------------------------------------------

// Mirando: pequeno salto a un punto cercano cada 2-4 s
inline EyeOffset eyeSaccadeAt(uint32_t msInEyes, uint32_t seed) {
  const EyeEvent e =
      eyeLatestEvent(msInEyes, seed, EyeSalt::SACCADE, 0, EYE_SACCADE_SLOT_MS, EYE_SACCADE_JITTER_MS);
  if (!e.isValid) {
    return {0, 0};
  }
  const EyeOffset to = eyeRandomOffset(eyeHash(seed, EyeSalt::SACCADE_POS, e.index), EYE_SACCADE_RADIUS);
  const EyeOffset from = e.index == 0
                             ? EyeOffset{0, 0}
                             : eyeRandomOffset(eyeHash(seed, EyeSalt::SACCADE_POS, e.index - 1), EYE_SACCADE_RADIUS);
  return eyeLerp(from, to, e.msSince, EYE_GLANCE_MS);
}

// Mirando alrededor, tramo k: uno de cada 3 es un circulo (sentido y punto de
// partida al azar); el resto, quedarse en una de 8 posiciones del borde
// (esquinas incluidas)
inline EyeOffset eyeWanderTarget(uint32_t variant, uint32_t k, uint32_t tInMs) {
  constexpr uint32_t KINDS = 3;
  constexpr float POSITIONS = 8.0F;
  const uint32_t h = eyeHash(variant, EyeSalt::WANDER, k);
  const float startAngle = EYE_TWO_PI * static_cast<float>((h >> 8) & 7U) / POSITIONS;
  if (h % KINDS != 0) {
    return eyePolar(startAngle, EYE_ORBIT_RADIUS);
  }
  const float direction = ((h >> 16) & 1U) != 0 ? 1.0F : -1.0F;
  const float turn = EYE_TWO_PI * static_cast<float>(tInMs) / static_cast<float>(EYE_ORBIT_PERIOD_MS);
  return eyePolar(startAngle + direction * turn, EYE_ORBIT_RADIUS);
}

inline EyeOffset eyeWanderAt(uint32_t msInMood, uint32_t variant) {
  const uint32_t k = msInMood / EYE_WANDER_SEGMENT_MS;
  const uint32_t t = msInMood % EYE_WANDER_SEGMENT_MS;
  const EyeOffset from = k == 0 ? EyeOffset{0, 0} : eyeWanderTarget(variant, k - 1, EYE_WANDER_SEGMENT_MS);
  return eyeLerp(from, eyeWanderTarget(variant, k, t), t, EYE_WANDER_EASE_MS);
}

// Pensando, en pasos de 1,5 s: sube, va a un lado (al azar), se queda y vuelve
inline EyeOffset eyeThinkingAt(uint32_t msInMood, uint32_t variant) {
  constexpr uint32_t STEPS = 4;
  const int16_t side = (variant & 1U) != 0 ? EYE_THINK_SIDE_PX : static_cast<int16_t>(-EYE_THINK_SIDE_PX);
  const EyeOffset center{0, 0};
  const EyeOffset up{0, static_cast<int16_t>(-EYE_THINK_UP_PX)};
  const EyeOffset aside{side, static_cast<int16_t>(-EYE_THINK_UP_PX)};
  const uint32_t t = msInMood % EYE_THINK_STEP_MS;
  switch ((msInMood / EYE_THINK_STEP_MS) % STEPS) {
    case 0:
      return eyeLerp(center, up, t, EYE_THINK_STEP_MS);
    case 1:
      return eyeLerp(up, aside, t, EYE_THINK_STEP_MS);
    case 2:
      return aside;
    default:
      return eyeLerp(aside, center, t, EYE_THINK_STEP_MS);
  }
}

// ----- Caras ------------------------------------------------------------------------

// Sorpresa: ojos un 20 % mas grandes, pupila a la mitad y temblando
inline EyeFrame eyeSurprisedFrame(uint32_t msInEyes, uint32_t seed) {
  const uint32_t h = eyeHash(seed, EyeSalt::JITTER, msInEyes / EYE_JITTER_STEP_MS);
  EyeShape s = eyeOpen(EYE_FULL_PCT, eyeRandomOffset(h, EYE_SURPRISE_JITTER_PX));
  s.scalePct = EYE_SURPRISE_SCALE_PCT;
  s.pupilPct = EYE_SURPRISE_PUPIL_PCT;
  return eyeBoth(s);
}

// Dormido: lineas que suben y bajan como al respirar
inline EyeFrame eyeSleepingFrame(uint32_t msInMood) {
  EyeShape s;
  s.form = EyeForm::LINE;
  s.dy = eyeWave(msInMood, EYE_SLEEP_BREATH_MS, EYE_SLEEP_BREATH_PX);
  return eyeBoth(s);
}

// Cansado: parpado a media altura y pupila baja. En cada ciclo de 8 s el
// parpado baja y vuelve a subir despacio (micro-sueno).
inline EyeFrame eyeTiredFrame(uint32_t msInMood) {
  constexpr uint32_t HALF = EYE_TIRED_DROOP_MS / 2;
  const uint32_t t = msInMood % EYE_TIRED_CYCLE_MS;
  uint32_t droopPct = 0;
  if (t >= EYE_TIRED_DROOP_FROM_MS && t < EYE_TIRED_DROOP_FROM_MS + EYE_TIRED_DROOP_MS) {
    const uint32_t x = t - EYE_TIRED_DROOP_FROM_MS;
    droopPct = x < HALF ? x * EYE_FULL_PCT / HALF : (EYE_TIRED_DROOP_MS - x) * EYE_FULL_PCT / HALF;
  }
  EyeShape s = eyeOpen(EYE_FULL_PCT, {0, EYE_TIRED_PUPIL_DY});
  s.lidPct = static_cast<uint8_t>(EYE_TIRED_LID_PCT + (EYE_TIRED_DROOP_LID_PCT - EYE_TIRED_LID_PCT) * droopPct /
                                                          EYE_FULL_PCT);
  return eyeBoth(s);
}

// Confundido: un ojo grande y otro pequeno, pupilas arriba; cada 3 s se cambian
inline EyeFrame eyeConfusedFrame(uint32_t msInMood, uint32_t variant, uint8_t openPct) {
  const bool isLeftBig = (((msInMood / EYE_CONFUSED_SWAP_MS) + variant) & 1U) == 0;
  EyeShape big = eyeOpen(openPct, {0, EYE_CONFUSED_PUPIL_DY});
  big.scalePct = EYE_CONFUSED_BIG_PCT;
  EyeShape small = eyeOpen(openPct, {0, EYE_CONFUSED_PUPIL_DY});
  small.scalePct = EYE_CONFUSED_SMALL_PCT;
  return isLeftBig ? EyeFrame{big, small} : EyeFrame{small, big};
}

// Ojo que guina: arco, se abre, parpadea una vez y queda abierto
inline EyeShape eyeWinkingEye(uint32_t msInMood) {
  if (msInMood < EYE_WINK_CLOSED_MS) {
    return eyeArc(EYE_FULL_PCT, 0);
  }
  const uint32_t t = msInMood - EYE_WINK_CLOSED_MS;
  if (t < EYE_WINK_OPENING_MS) {
    return eyeOpen(static_cast<uint8_t>(t * EYE_FULL_PCT / EYE_WINK_OPENING_MS), {0, 0});
  }
  return eyeOpen(eyeBlinkOpenPct(t - EYE_WINK_OPENING_MS, EYE_BLINK_MS, EYE_BLINK_CLOSED_MS), {0, 0});
}

// Guinando: el ojo que guina (izquierdo o derecho) sale al azar
inline EyeFrame eyeWinkingFrame(uint32_t msInMood, uint32_t variant) {
  const EyeShape open = eyeOpen(EYE_FULL_PCT, {0, 0});
  const EyeShape winking = eyeWinkingEye(msInMood);
  return (variant & 1U) != 0 ? EyeFrame{open, winking} : EyeFrame{winking, open};
}

// Feliz: ^ ^ con una curva distinta en cada aparicion (80-120 %)
inline EyeFrame eyeHappyFrame(uint32_t variant) {
  constexpr uint32_t RANGE = EYE_HAPPY_MAX_ARC_PCT - EYE_HAPPY_MIN_ARC_PCT + 1;
  return eyeBoth(eyeArc(static_cast<uint8_t>(EYE_HAPPY_MIN_ARC_PCT + variant % RANGE), 0));
}

// Riendo: ^ ^ que se acentuan y aflojan, con una vibracion leve
inline EyeFrame eyeLaughingFrame(uint32_t msInMood) {
  const int16_t arc = EYE_LAUGH_ARC_PCT + eyeWave(msInMood, EYE_LAUGH_ARC_MS, EYE_LAUGH_ARC_SWING_PCT);
  return eyeBoth(eyeArc(static_cast<uint8_t>(arc), eyeWave(msInMood, EYE_LAUGH_SHAKE_MS, EYE_LAUGH_SHAKE_PX)));
}

// Enamorado: pupilas de corazon que laten (solo crecen, como un latido)
inline EyeFrame eyeInLoveFrame(uint32_t msInMood, uint8_t openPct) {
  const int16_t beat = eyeWave(msInMood, EYE_HEART_BEAT_MS, EYE_HEART_BEAT_PCT);
  EyeShape s = eyeOpen(openPct, {0, 0});
  s.pupil = PupilForm::HEART;
  s.pupilPct = static_cast<uint8_t>(EYE_FULL_PCT + (beat > 0 ? beat : 0));
  return eyeBoth(s);
}

// Fotograma de `scene` en el instante msInEyes (tiempo desde que se entro en
// los ojos o en el arranque). Misma entrada, mismo fotograma.
inline EyeFrame eyeFrameAt(const EyeScene& scene, uint32_t msInEyes, uint32_t seed) {
  const uint32_t t = scene.msInMood;
  switch (scene.mood) {
    case EyeMood::LOOKING:
      return eyeBoth(eyeOpen(eyeBlinkAt(msInEyes, seed, EyeBlinkStyle::NORMAL), eyeSaccadeAt(msInEyes, seed)));
    case EyeMood::BLINKING:
      return eyeBoth(eyeOpen(eyeBlinkAt(msInEyes, seed, EyeBlinkStyle::ALWAYS_DOUBLE), {0, 0}));
    case EyeMood::LOOKING_AROUND:
      return eyeBoth(eyeOpen(eyeBlinkAt(msInEyes, seed, EyeBlinkStyle::NORMAL), eyeWanderAt(t, scene.variant)));
    case EyeMood::HAPPY:
      return eyeHappyFrame(scene.variant);
    case EyeMood::SURPRISED:
      return eyeSurprisedFrame(msInEyes, seed);
    case EyeMood::SLEEPING:
      return eyeSleepingFrame(t);
    case EyeMood::TIRED:
      return eyeTiredFrame(t);
    case EyeMood::CONFUSED:
      return eyeConfusedFrame(t, scene.variant, eyeBlinkAt(msInEyes, seed, EyeBlinkStyle::NORMAL));
    case EyeMood::THINKING:
      return eyeBoth(eyeOpen(eyeBlinkAt(msInEyes, seed, EyeBlinkStyle::REDUCED), eyeThinkingAt(t, scene.variant)));
    case EyeMood::WINKING:
      return eyeWinkingFrame(t, scene.variant);
    case EyeMood::LAUGHING:
      return eyeLaughingFrame(t);
    case EyeMood::IN_LOVE:
      return eyeInLoveFrame(t, eyeBlinkAt(msInEyes, seed, EyeBlinkStyle::SOFT));
  }
  return eyeBoth(eyeOpen(EYE_FULL_PCT, {0, 0}));
}
