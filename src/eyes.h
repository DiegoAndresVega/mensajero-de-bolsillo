#pragma once

#include <cstdint>

#include "display.h"
#include "eye_shapes.h"

// Dibujo de los ojos (elipses, arcos, rectangulos y corazones, sin imagenes).
// Que cara toca y la forma de cada ojo es logica pura (eye_rules.h y
// eye_shapes.h); aqui solo se pinta un EyeFrame. Tamanos y colores en
// config.h (EYE_*). Solo lo usa la tarea de UI.

// Rectangulo de la pantalla donde se dibujan los ojos en cualquier cara.
// Fuera de el solo hay fondo, asi que basta con enviar este rectangulo.
struct EyesBand {
  int32_t x;
  int32_t y;
  int32_t w;
  int32_t h;
};

EyesBand eyesBand();

// Borra el rectangulo de los ojos y dibuja el fotograma en el canvas.
void eyesDraw(LGFX_Sprite& canvas, const EyeFrame& frame);
