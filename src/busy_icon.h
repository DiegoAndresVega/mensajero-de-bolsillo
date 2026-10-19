#pragma once

#include "audio_view.h"

// Icono de "ocupado" (reloj de arena) en la esquina superior derecha. Se
// dibuja una sola vez en su propio sprite: el framebuffer no se toca, asi que
// al quitar el icono basta con reenviar esa esquina del canvas. Solo lo usa
// la tarea de UI (ui_manager decide cuando se ve).

// Crea y pinta el sprite (2,6 KB en SRAM interna). false si no hay memoria:
// se sigue sin icono.
bool busyIconInit();
// Envia el icono a la pantalla (menos de 1 ms). No hace nada sin sprite.
void busyIconPush();
// Rectangulo que ocupa en la pantalla
ScreenRect busyIconRect();
