#pragma once

#include <cstdint>

#include "audio_rules.h"
#include "display.h"
#include "message_rules.h"

// Visor de un mensaje de voz: autor en la cabecera con el corazon de
// favorito, titulo y fecha, una onda decorativa de barras rosas y abajo
// play/pausa con el tiempo. Tras cambiar el volumen se ve 2 s una barra de
// volumen en lugar del titulo. Los botones son logica pura
// (audioViewOnButton en audio_view_rules.h); sonar es cosa de audio_player.
// Solo lo usa la tarea de UI.

// Lo que se pinta, salvo la onda. Si no cambia, no hay que repintar.
struct AudioViewModel {
  PlayerState state;
  uint32_t positionSec;
  uint32_t durationSec;
  bool isVolumeShown;
  uint8_t volume;
  bool isFavorite;
  bool isOnline;
};

bool operator==(const AudioViewModel& a, const AudioViewModel& b);

// Rectangulo de la pantalla
struct ScreenRect {
  int32_t x;
  int32_t y;
  int32_t w;
  int32_t h;
};

// No lee la SD: todo sale del resumen de la lista. El MP3 es /audios/{id}.mp3
// (o /papelera/audios/{id}.mp3 con isInTrash, y el pie no ofrece favorito).
void audioViewOpen(const MessageSummary& msg, bool isInTrash = false);
const char* audioViewId();
const char* audioViewPath();
// Tras cambiar el favorito en la lista (messageListToggleFavorite)
void audioViewSetFavorite(bool isFavorite);
bool audioViewIsFavorite();

// Pantalla completa. waveTick elige las alturas de la onda: el mismo tick da
// la misma onda (en pausa no se mueve).
void audioViewDraw(LGFX_Sprite& canvas, const AudioViewModel& model, uint32_t waveTick);

// Solo la onda, para enviar solo su rectangulo mientras suena
ScreenRect audioViewWavesRect();
void audioViewDrawWaves(LGFX_Sprite& canvas, uint32_t waveTick);

// Solo play/pausa y el tiempo, para el cambio de segundo
ScreenRect audioViewControlsRect();
void audioViewDrawControls(LGFX_Sprite& canvas, const AudioViewModel& model);
