#pragma once

#include <cstdint>

#include "display.h"

// Pantalla Fotos del menu principal: los JPEG de /fotos en la SD (copiados a
// mano, preparados con tools/fotos/preparar.sh), uno a pantalla completa.
// Los botones son logica pura (photosOnButton en photo_rules.h). Solo lo usa
// la tarea de UI.

// Lee los nombres de /fotos la primera vez; despues no hace nada (la SD no
// se cambia en caliente). Con el bus libre: bloquea la SD mientras lee.
void photosLoadOnce();

uint16_t photosCount();

// Pantalla completa: la foto `index`, o un aviso si no hay fotos o no se
// puede abrir. Lee el JPEG de la SD (bloqueandola) y lo decodifica en el canvas.
void photosDraw(LGFX_Sprite& canvas, uint16_t index, bool isOnline);
