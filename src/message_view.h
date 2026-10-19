#pragma once

#include <cstdint>

#include "display.h"
#include "message_rules.h"

// Visor de un mensaje de texto: autor en la cabecera con el corazon de
// favorito, fecha debajo y, con scroll, el titulo entero en negrita y el
// contenido, los dos partidos en lineas. El scroll es logica pura
// (messageViewOnButton en message_rules.h). Solo lo usa la tarea de UI.
// Los mensajes de voz van a audio_view.

// Lee el contenido de la SD (bloquea un momento) y lo parte en lineas con la
// fuente del visor. `canvas` solo se usa para medir el texto. isInTrash: el
// JSON esta en la papelera (y el pie no ofrece favorito).
void messageViewOpen(LGFX_Sprite& canvas, const MessageSummary& msg, bool isInTrash = false);

// Id del mensaje abierto
const char* messageViewId();
// Tras cambiar el favorito en la lista (messageListToggleFavorite)
void messageViewSetFavorite(bool isFavorite);

uint16_t messageViewLineCount();
uint16_t messageViewVisibleLines();

// Pinta la pantalla completa empezando en la linea topLine del contenido
void messageViewDraw(LGFX_Sprite& canvas, uint16_t topLine, bool isOnline);
