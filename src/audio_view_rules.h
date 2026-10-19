#pragma once

// Logica pura del visor de audio, sin dependencias de Arduino: se compila
// tambien en el entorno native para los tests (pio test -e native). Dibujar
// es cosa de audio_view; sonar, de audio_player.

#include <cstddef>
#include <cstdint>
#include <cstdio>

#include "audio_rules.h"
#include "button_rules.h"

// ----- Botones ------------------------------------------------------------------

enum class AudioViewAction : uint8_t {
  None,
  Play,            // desde el segundo 0
  Pause,
  Resume,
  Stop,            // vuelve al segundo 0 y sigue en el visor
  Back,            // a la lista (parando el audio si estaba en pausa)
  VolumeUp,
  VolumeDown,
  ToggleFavorite
};

// 1/2 volumen (solo sonando), 3 play/pausa, 4 stop si suena y si no vuelve a
// la lista. El 3 largo marca favorito: el corto ya es play/pausa. Un doble
// toque cuenta como pulsacion normal.
inline AudioViewAction audioViewOnButton(PlayerState state, const ButtonEvent& ev) {
  const bool isPlaying = state == PlayerState::Playing;
  if (ev.longPress) {
    return ev.button == Button::BTN3_OK ? AudioViewAction::ToggleFavorite : AudioViewAction::None;
  }
  switch (ev.button) {
    case Button::BTN1_UP:
      return isPlaying ? AudioViewAction::VolumeUp : AudioViewAction::None;
    case Button::BTN2_DOWN:
      return isPlaying ? AudioViewAction::VolumeDown : AudioViewAction::None;
    case Button::BTN3_OK:
      if (isPlaying) {
        return AudioViewAction::Pause;
      }
      return state == PlayerState::Paused ? AudioViewAction::Resume : AudioViewAction::Play;
    case Button::BTN4_BACK:
      return isPlaying ? AudioViewAction::Stop : AudioViewAction::Back;
    case Button::NONE:
      break;
  }
  return AudioViewAction::None;
}

// ----- Tiempo -------------------------------------------------------------------

// "MM:SS / MM:SS", o solo "MM:SS" si no se sabe la duracion (0). La posicion
// no pasa de la duracion: la libreria redondea y puede ir un segundo por delante.
inline void formatPlayTime(uint32_t positionSec, uint32_t durationSec, char* out, size_t outSize) {
  if (outSize == 0) {
    return;
  }
  constexpr uint32_t SEC_PER_MIN = 60;
  constexpr uint32_t MAX_SHOWN_SEC = 99 * SEC_PER_MIN + 59;
  auto clampShown = [](uint32_t sec) { return sec < MAX_SHOWN_SEC ? sec : MAX_SHOWN_SEC; };
  const uint32_t pos = clampShown(durationSec > 0 && positionSec > durationSec ? durationSec : positionSec);
  if (durationSec == 0) {
    snprintf(out, outSize, "%02u:%02u", static_cast<unsigned>(pos / SEC_PER_MIN),
             static_cast<unsigned>(pos % SEC_PER_MIN));
    return;
  }
  const uint32_t dur = clampShown(durationSec);
  snprintf(out, outSize, "%02u:%02u / %02u:%02u", static_cast<unsigned>(pos / SEC_PER_MIN),
           static_cast<unsigned>(pos % SEC_PER_MIN), static_cast<unsigned>(dur / SEC_PER_MIN),
           static_cast<unsigned>(dur % SEC_PER_MIN));
}

// ----- Ondas --------------------------------------------------------------------

inline uint32_t waveHash(uint32_t bar, uint32_t tick) {
  uint32_t x = (bar * 0x9E3779B9U) ^ (tick * 0x85EBCA6BU);
  x ^= x >> 16;
  x *= 0x7FEB352DU;
  x ^= x >> 15;
  x *= 0x846CA68BU;
  x ^= x >> 16;
  return x;
}

// Alto de la barra `bar` (de barCount) en el fotograma `tick`, entre minH y
// maxH. Decorativo: no analiza el MP3. Las del centro suben mas que las de
// los lados, como una onda de voz. Mismo tick, mismas alturas: en pausa no
// se mueven.
inline int32_t waveBarHeight(uint32_t bar, uint32_t barCount, uint32_t tick, int32_t minH, int32_t maxH) {
  constexpr uint32_t PCT = 100;
  // Las barras de los extremos llegan como mucho a este % del alto
  constexpr uint32_t EDGE_PCT = 35;
  if (barCount == 0 || maxH <= minH) {
    return minH;
  }
  const uint32_t center2 = barCount - 1;  // el centro, por 2
  const uint32_t dist2 = bar * 2 > center2 ? bar * 2 - center2 : center2 - bar * 2;
  const uint32_t envelopePct = center2 == 0 ? PCT : PCT - (PCT - EDGE_PCT) * dist2 / center2;
  const uint32_t randomPct = waveHash(bar, tick) % (PCT + 1);
  const int32_t range = maxH - minH;
  return minH + static_cast<int32_t>(static_cast<uint32_t>(range) * envelopePct / PCT * randomPct / PCT);
}

// Ancho de la parte llena de la barra de volumen
inline int32_t volumeFillWidth(uint8_t volume, int32_t trackWidth) {
  const uint8_t v = clampAudioVolume(volume);
  return trackWidth * v / AUDIO_VOLUME_MAX;
}
