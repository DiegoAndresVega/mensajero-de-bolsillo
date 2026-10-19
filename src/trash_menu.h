#pragma once

#include <cstdint>

#include "display.h"

// Dibujo del menu de la Papelera: Ver mensajes / Seleccionar / Vaciar
// papelera y, si hay cola pendiente, Cancelar borrado pendiente (N). La
// navegacion es logica pura (trash_menu_rules.h). Solo lo usa la tarea de UI.
void trashMenuDraw(LGFX_Sprite& canvas, uint8_t selected, uint16_t pendingCount, bool isOnline);
