#pragma once

#include <cstdint>

#include "audio_view.h"
#include "display.h"

// Aviso de mensaje nuevo: un sobre blanco con contorno negro y punto rojo en
// una tarjeta en el centro, vibrando a los lados. Tiempos y botones son
// logica pura (popup_rules.h); cuando se ve y que pasa con el audio lo decide
// ui_manager. Solo lo usa la tarea de UI.

// Rectangulo de la tarjeta: lo unico que hay que enviar en cada fotograma
ScreenRect notificationPopupRect();

// Pinta la tarjeta con el sobre desplazado offsetX pixeles a los lados.
// Lo que hay fuera de la tarjeta no se toca.
void notificationPopupDraw(LGFX_Sprite& canvas, int32_t offsetX);
