#pragma once

// Reglas de los ojos sin dependencias de Arduino: se compilan tambien en el
// entorno native para los tests (pio test -e native). Deciden que cara toca y
// cuando parpadea; la forma de cada ojo esta en eye_shapes.h y dibujar es
// cosa de eyes.cpp.
//
// El azar sale de una semilla (nueva cada vez que se entra en los ojos)
// pasada por un hash. Todo es funcion pura del tiempo: el mismo instante con
// la misma semilla da siempre el mismo fotograma, sin estado escondido.

#include <cmath>
#include <cstdint>

// ----- Caras --------------------------------------------------------------------

enum class EyeMood : uint8_t {
  LOOKING,         // mirando: abiertos; la pupila hace micro-movimientos cada 2-4 s
  BLINKING,        // parpadeando: pupila quieta; cada parpadeo es doble
  LOOKING_AROUND,  // mirando alrededor: tramos de circulos o saltos a esquinas
  HAPPY,           // feliz: ^ ^ con la curva un poco distinta cada vez
  SURPRISED,       // sorpresa: ojos grandes y pupila pequena que tiembla
  SLEEPING,        // dormido: dos lineas que suben y bajan despacio
  TIRED,           // cansado: parpado a media altura; cada 8 s casi se cierra
  CONFUSED,        // confundido: un ojo grande y otro pequeno, pupilas arriba
  THINKING,        // pensando: mira arriba, luego a un lado y vuelve; parpadea poco
  WINKING,         // guinando: un ojo en arco que parpadea una vez antes de abrirse
  LAUGHING,        // riendo: ^ ^ que vibran y se acentuan
  IN_LOVE          // enamorado: pupilas de corazon que laten; parpadeo lento
};
constexpr uint8_t EYE_MOOD_COUNT = 12;

inline const char* eyeMoodName(EyeMood mood) {
  switch (mood) {
    case EyeMood::LOOKING:
      return "mirando";
    case EyeMood::BLINKING:
      return "parpadeando";
    case EyeMood::LOOKING_AROUND:
      return "mirando alrededor";
    case EyeMood::HAPPY:
      return "feliz";
    case EyeMood::SURPRISED:
      return "sorpresa";
    case EyeMood::SLEEPING:
      return "dormido";
    case EyeMood::TIRED:
      return "cansado";
    case EyeMood::CONFUSED:
      return "confundido";
    case EyeMood::THINKING:
      return "pensando";
    case EyeMood::WINKING:
      return "guinando";
    case EyeMood::LAUGHING:
      return "riendo";
    case EyeMood::IN_LOVE:
      return "enamorado";
  }
  return "?";
}

// ----- Programa segun el tiempo en los ojos (el apagado llega a los 60 s) ----------

constexpr uint32_t EYE_WANDER_FROM_MS = 15000;  // 0-15 s: mirando / parpadeando
constexpr uint32_t EYE_TIRED_FROM_MS = 30000;   // 15-30 s: alrededor / pensando
constexpr uint32_t EYE_SLEEP_FROM_MS = 45000;   // 30-45 s: cansado / confundido; luego dormido
// Dentro de cada fase las dos caras se turnan (el orden, al azar)
constexpr uint32_t EYE_ALTERNATE_MS = 7500;

// Micro-expresion cada 10-20 s: la k-esima empieza en 5 s + k * 15 s + [0, 5 s).
// Solo si acaba antes de dormir (dormido no hace muecas).
constexpr uint32_t EYE_MICRO_FIRST_MS = 5000;
constexpr uint32_t EYE_MICRO_SLOT_MS = 15000;
constexpr uint32_t EYE_MICRO_JITTER_MS = 5000;
constexpr uint32_t EYE_MICRO_MS = 1000;
constexpr uint32_t EYE_WINK_MS = 1000;
constexpr uint32_t EYE_LAUGH_MS = 1500;

// ----- Parpadeo ---------------------------------------------------------------

constexpr uint32_t EYE_BLINK_MS = 120;
// Parte del parpadeo con el ojo cerrado del todo. Mas larga que un fotograma
// para que siempre se llegue a ver cerrado (si no, a 30 FPS puede saltarse).
constexpr uint32_t EYE_BLINK_CLOSED_MS = 40;
// Enamorado: parpadeo lento
constexpr uint32_t EYE_SOFT_BLINK_MS = 300;
// Un parpadeo por hueco de 4,5 s, en un momento al azar de su primer 1,5 s:
// entre dos parpadeos pasan de 3 a 6 s
constexpr uint32_t EYE_BLINK_SLOT_MS = 4500;
constexpr uint32_t EYE_BLINK_JITTER_MS = 1500;
constexpr uint32_t EYE_DOUBLE_BLINK_ONE_IN = 4;
constexpr uint32_t EYE_DOUBLE_BLINK_GAP_MS = 100;

// ----- Movimientos (pixeles y milisegundos) ---------------------------------------

constexpr int16_t EYE_SACCADE_RADIUS = 5;  // mirando
constexpr uint32_t EYE_SACCADE_SLOT_MS = 3000;
constexpr uint32_t EYE_SACCADE_JITTER_MS = 1000;
constexpr uint32_t EYE_GLANCE_MS = 150;  // lo que tarda la pupila en llegar
constexpr int16_t EYE_ORBIT_RADIUS = 18;  // mirando alrededor
constexpr uint32_t EYE_ORBIT_PERIOD_MS = 6000;
constexpr uint32_t EYE_WANDER_SEGMENT_MS = 2500;
constexpr uint32_t EYE_WANDER_EASE_MS = 300;
constexpr uint8_t EYE_SURPRISE_SCALE_PCT = 120;
constexpr uint8_t EYE_SURPRISE_PUPIL_PCT = 50;
constexpr int16_t EYE_SURPRISE_JITTER_PX = 2;
constexpr uint32_t EYE_JITTER_STEP_MS = 66;
constexpr int16_t EYE_SLEEP_BREATH_PX = 3;
constexpr uint32_t EYE_SLEEP_BREATH_MS = 4000;
constexpr uint8_t EYE_TIRED_LID_PCT = 45;
constexpr uint8_t EYE_TIRED_DROOP_LID_PCT = 80;
constexpr uint32_t EYE_TIRED_CYCLE_MS = 8000;
constexpr uint32_t EYE_TIRED_DROOP_FROM_MS = 3000;
constexpr uint32_t EYE_TIRED_DROOP_MS = 3000;
constexpr int16_t EYE_TIRED_PUPIL_DY = 16;
constexpr uint8_t EYE_CONFUSED_BIG_PCT = 125;
constexpr uint8_t EYE_CONFUSED_SMALL_PCT = 85;
constexpr uint32_t EYE_CONFUSED_SWAP_MS = 3000;
constexpr int16_t EYE_CONFUSED_PUPIL_DY = -10;
constexpr int16_t EYE_THINK_UP_PX = 14;
constexpr int16_t EYE_THINK_SIDE_PX = 14;
constexpr uint32_t EYE_THINK_STEP_MS = 1500;  // arriba, lado, quieto, vuelta
constexpr uint32_t EYE_WINK_CLOSED_MS = 550;
constexpr uint32_t EYE_WINK_OPENING_MS = 100;
constexpr uint8_t EYE_HAPPY_MIN_ARC_PCT = 80;
constexpr uint8_t EYE_HAPPY_MAX_ARC_PCT = 120;
constexpr uint8_t EYE_LAUGH_ARC_PCT = 110;
constexpr int16_t EYE_LAUGH_ARC_SWING_PCT = 20;
constexpr uint32_t EYE_LAUGH_ARC_MS = 500;
constexpr int16_t EYE_LAUGH_SHAKE_PX = 2;
constexpr uint32_t EYE_LAUGH_SHAKE_MS = 120;
constexpr int16_t EYE_HEART_BEAT_PCT = 15;
constexpr uint32_t EYE_HEART_BEAT_MS = 800;
// Lo mas grande que se pone un ojo (confundido) y lo mas alto de un arco (risa)
constexpr uint8_t EYE_MAX_SCALE_PCT = EYE_CONFUSED_BIG_PCT;
constexpr uint8_t EYE_MAX_ARC_PCT = EYE_LAUGH_ARC_PCT + EYE_LAUGH_ARC_SWING_PCT;

// ----- Forma de un ojo en un fotograma --------------------------------------------

enum class EyeForm : uint8_t {
  OPEN,  // ovalo con pupila
  ARC,   // ^ cerrado (feliz, risa, guino)
  LINE   // linea horizontal (dormido)
};

enum class PupilForm : uint8_t { ROUND, HEART };

struct EyeShape {
  EyeForm form = EyeForm::OPEN;
  PupilForm pupil = PupilForm::ROUND;
  uint8_t scalePct = 100;  // tamano del ojo
  uint8_t openPct = 100;   // 100 = abierto, 0 = cerrado (parpadeo)
  uint8_t lidPct = 0;      // parte de arriba tapada por el parpado (cansado)
  uint8_t pupilPct = 100;  // tamano de la pupila
  uint8_t arcPct = 100;    // altura del arco ^ respecto a un semicirculo
  int16_t pupilDx = 0;
  int16_t pupilDy = 0;
  int16_t dy = 0;  // desplazamiento vertical del ojo entero
};

inline bool operator==(const EyeShape& a, const EyeShape& b) {
  return a.form == b.form && a.pupil == b.pupil && a.scalePct == b.scalePct && a.openPct == b.openPct &&
         a.lidPct == b.lidPct && a.pupilPct == b.pupilPct && a.arcPct == b.arcPct && a.pupilDx == b.pupilDx &&
         a.pupilDy == b.pupilDy && a.dy == b.dy;
}

// Lo que se dibuja. Si dos fotogramas son iguales no hace falta reenviarlos.
struct EyeFrame {
  EyeShape left;
  EyeShape right;
};

inline bool operator==(const EyeFrame& a, const EyeFrame& b) {
  return a.left == b.left && a.right == b.right;
}

// Cara en pantalla y cuanto lleva puesta. variant es un numero al azar por
// aparicion (curva de la sonrisa, ojo que guina, recorrido de la mirada...).
struct EyeScene {
  EyeMood mood;
  uint32_t msInMood;
  uint32_t variant;
};

// ----- Azar -------------------------------------------------------------------------

// Tipos de sorteo: cada uno con su serie de numeros para que no se mezclen
enum class EyeSalt : uint32_t {
  ORDER = 1,
  VARIANT,
  MICRO,
  MICRO_MOOD,
  BLINK,
  DOUBLE_BLINK,
  SACCADE,
  SACCADE_POS,
  WANDER,
  JITTER
};

inline uint32_t eyeHash(uint32_t seed, EyeSalt salt, uint32_t index) {
  uint32_t x = seed ^ (static_cast<uint32_t>(salt) * 0x9E3779B9U) ^ (index * 0x85EBCA6BU);
  x ^= x >> 16;
  x *= 0x7FEB352DU;
  x ^= x >> 15;
  x *= 0x846CA68BU;
  x ^= x >> 16;
  return x;
}

// Sucesos al azar: el k-esimo empieza en firstMs + k * slotMs + [0, jitterMs).
// Devuelve el ultimo que ya ha empezado en ms. Requiere 0 < jitterMs <= slotMs.
struct EyeEvent {
  bool isValid;
  uint32_t index;
  uint32_t msSince;
};

inline EyeEvent eyeLatestEvent(uint32_t ms, uint32_t seed, EyeSalt salt, uint32_t firstMs, uint32_t slotMs,
                               uint32_t jitterMs) {
  if (ms < firstMs) {
    return {false, 0, 0};
  }
  const uint32_t k = (ms - firstMs) / slotMs;
  const uint32_t start = firstMs + k * slotMs + eyeHash(seed, salt, k) % jitterMs;
  if (start <= ms) {
    return {true, k, ms - start};
  }
  if (k == 0) {
    return {false, 0, 0};
  }
  // El anterior empezo antes de firstMs + k * slotMs <= ms
  const uint32_t prevStart = firstMs + (k - 1) * slotMs + eyeHash(seed, salt, k - 1) % jitterMs;
  return {true, k - 1, ms - prevStart};
}

// ----- Que cara toca --------------------------------------------------------------

inline uint32_t eyeMoodDurationMs(EyeMood mood) {
  switch (mood) {
    case EyeMood::WINKING:
      return EYE_WINK_MS;
    case EyeMood::LAUGHING:
      return EYE_LAUGH_MS;
    default:
      return EYE_MICRO_MS;
  }
}

// Las dos caras de una fase se turnan cada EYE_ALTERNATE_MS
inline EyeScene eyeAlternate(EyeMood a, EyeMood b, uint32_t msInPhase, uint32_t phase, uint32_t seed) {
  const uint32_t slot = msInPhase / EYE_ALTERNATE_MS;
  const bool isFirstB = (eyeHash(seed, EyeSalt::ORDER, phase) & 1U) != 0;
  const bool isB = ((slot & 1U) != 0) != isFirstB;
  return {isB ? b : a, msInPhase % EYE_ALTERNATE_MS, eyeHash(seed, EyeSalt::VARIANT, phase * 64 + slot)};
}

// Cara de fondo segun la fase, sin micro-expresiones
inline EyeScene eyeBaseScene(uint32_t msInEyes, uint32_t seed) {
  if (msInEyes < EYE_WANDER_FROM_MS) {
    return eyeAlternate(EyeMood::LOOKING, EyeMood::BLINKING, msInEyes, 0, seed);
  }
  if (msInEyes < EYE_TIRED_FROM_MS) {
    return eyeAlternate(EyeMood::LOOKING_AROUND, EyeMood::THINKING, msInEyes - EYE_WANDER_FROM_MS, 1, seed);
  }
  if (msInEyes < EYE_SLEEP_FROM_MS) {
    return eyeAlternate(EyeMood::TIRED, EyeMood::CONFUSED, msInEyes - EYE_TIRED_FROM_MS, 2, seed);
  }
  return {EyeMood::SLEEPING, msInEyes - EYE_SLEEP_FROM_MS, 0};
}

// Que cara poner en los ojos. Sorpresa (acaba de llegar un mensaje) manda;
// luego una micro-expresion si toca, y si no la cara de la fase.
inline EyeScene eyeSceneAt(uint32_t msInEyes, bool isSurprised, uint32_t seed) {
  if (isSurprised) {
    return {EyeMood::SURPRISED, msInEyes, seed};
  }
  static constexpr EyeMood MICRO_MOODS[] = {EyeMood::HAPPY, EyeMood::SURPRISED, EyeMood::WINKING, EyeMood::LAUGHING,
                                            EyeMood::IN_LOVE};
  constexpr uint32_t MICRO_COUNT = sizeof(MICRO_MOODS) / sizeof(MICRO_MOODS[0]);
  const EyeEvent e =
      eyeLatestEvent(msInEyes, seed, EyeSalt::MICRO, EYE_MICRO_FIRST_MS, EYE_MICRO_SLOT_MS, EYE_MICRO_JITTER_MS);
  if (e.isValid) {
    const EyeMood mood = MICRO_MOODS[eyeHash(seed, EyeSalt::MICRO_MOOD, e.index) % MICRO_COUNT];
    const uint32_t durationMs = eyeMoodDurationMs(mood);
    const uint32_t startMs = msInEyes - e.msSince;
    if (e.msSince < durationMs && startMs + durationMs <= EYE_SLEEP_FROM_MS) {
      return {mood, e.msSince, eyeHash(seed, EyeSalt::VARIANT, 0x8000U + e.index)};
    }
  }
  return eyeBaseScene(msInEyes, seed);
}

// Arranque: feliz, con su propia curva al azar
inline EyeScene eyeBootScene(uint32_t msInBoot, uint32_t seed) {
  return {EyeMood::HAPPY, msInBoot, seed};
}

// ----- Parpadeo ---------------------------------------------------------------------

// Apertura del ojo (100 = abierto, 0 = cerrado) a msSinceStart del inicio de
// un parpadeo de blinkMs: cierra, se queda cerrado closedMs y vuelve a abrir.
// El tramo cerrado garantiza que se vea al menos un fotograma con el ojo cerrado.
inline uint8_t eyeBlinkOpenPct(uint32_t msSinceStart, uint32_t blinkMs, uint32_t closedMs) {
  constexpr uint32_t OPEN = 100;
  if (msSinceStart >= blinkMs) {
    return OPEN;
  }
  const uint32_t rampMs = closedMs >= blinkMs ? 0 : (blinkMs - closedMs) / 2;
  if (msSinceStart < rampMs) {
    return static_cast<uint8_t>(OPEN - msSinceStart * OPEN / rampMs);
  }
  const uint32_t openingStartMs = rampMs + closedMs;
  if (msSinceStart < openingStartMs) {
    return 0;
  }
  const uint32_t openingMs = blinkMs - openingStartMs;
  return static_cast<uint8_t>((msSinceStart - openingStartMs) * OPEN / openingMs);
}

enum class EyeBlinkStyle : uint8_t {
  NORMAL,         // cada 3-6 s; uno de cada 4, doble
  ALWAYS_DOUBLE,  // parpadeando
  REDUCED,        // pensando: se salta uno de cada dos (cada 6-12 s)
  SOFT            // enamorado: mas lento
};

// Apertura por parpadeo en el instante msInEyes. La serie de parpadeos es la
// misma para todas las caras, asi que no se corta al cambiar de cara.
inline uint8_t eyeBlinkAt(uint32_t msInEyes, uint32_t seed, EyeBlinkStyle style) {
  const EyeEvent e =
      eyeLatestEvent(msInEyes, seed, EyeSalt::BLINK, 0, EYE_BLINK_SLOT_MS, EYE_BLINK_JITTER_MS);
  if (!e.isValid || (style == EyeBlinkStyle::REDUCED && (e.index & 1U) != 0)) {
    return 100;
  }
  const uint32_t blinkMs = style == EyeBlinkStyle::SOFT ? EYE_SOFT_BLINK_MS : EYE_BLINK_MS;
  if (e.msSince < blinkMs) {
    return eyeBlinkOpenPct(e.msSince, blinkMs, EYE_BLINK_CLOSED_MS);
  }
  const bool isDouble = style == EyeBlinkStyle::ALWAYS_DOUBLE ||
                        (style == EyeBlinkStyle::NORMAL &&
                         eyeHash(seed, EyeSalt::DOUBLE_BLINK, e.index) % EYE_DOUBLE_BLINK_ONE_IN == 0);
  const uint32_t secondStartMs = blinkMs + EYE_DOUBLE_BLINK_GAP_MS;
  if (!isDouble || e.msSince < secondStartMs) {
    return 100;
  }
  return eyeBlinkOpenPct(e.msSince - secondStartMs, blinkMs, EYE_BLINK_CLOSED_MS);
}
