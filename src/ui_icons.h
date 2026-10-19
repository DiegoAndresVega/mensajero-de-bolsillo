#pragma once

#include <cstdint>

#include "display.h"

// Iconos procedurales comunes a varias pantallas (sin imagenes). Solo los usa
// la tarea de UI.

// Corazon de ancho `size` centrado en (cx, cy): dos circulos y un triangulo
void uiDrawHeart(LGFX_Sprite& canvas, int32_t cx, int32_t cy, int32_t size, uint32_t color);

// Solo el contorno: corazon de `color` con otro mas pequeno de `bgColor` dentro
void uiDrawHeartOutline(LGFX_Sprite& canvas, int32_t cx, int32_t cy, int32_t size, uint32_t color,
                        uint32_t bgColor);

// Corazon de favorito en la esquina superior derecha de los visores: rosa
// relleno si lo es, contorno gris si no
void uiDrawFavoriteBadge(LGFX_Sprite& canvas, bool isFavorite);

// Circulo de `color` con una "i" hueca, centrado en (cx, cy)
void uiDrawInfoIcon(LGFX_Sprite& canvas, int32_t cx, int32_t cy, int32_t size, uint32_t color, uint32_t bgColor);

// Foto de ancho `size` centrada en (cx, cy): marco con una montana y un sol
// del color de fondo
void uiDrawPhotoIcon(LGFX_Sprite& canvas, int32_t cx, int32_t cy, int32_t size, uint32_t color, uint32_t bgColor);

// Papelera de lado `size` centrada en (cx, cy): tapa con asa y cubo con tres
// ranuras del color de fondo
void uiDrawTrashIcon(LGFX_Sprite& canvas, int32_t cx, int32_t cy, int32_t size, uint32_t color, uint32_t bgColor);

// Altavoz dentro del rectangulo (x, y, w, h): el iman y el cono abriendose a
// la derecha
void uiDrawSpeaker(LGFX_Sprite& canvas, int32_t x, int32_t y, int32_t w, int32_t h, uint32_t color);
