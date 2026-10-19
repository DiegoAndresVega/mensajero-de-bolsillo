#pragma once

#include <cstdint>

#include "display.h"

// Pantalla Info del menu principal: el mensaje personal del propietario
// (personal_message de config.json) partido en lineas, con scroll. Los
// botones son logica pura (infoOnButton en info_rules.h). Solo lo usa la
// tarea de UI.

// Prepara el texto: pasa a Latin-1, corta a INFO_MAX_CHARS y lo parte en
// lineas. `canvas` solo se usa para medir. Una vez, en uiInit.
void infoScreenInit(LGFX_Sprite& canvas, const char* personalMessageUtf8);

uint16_t infoScreenLineCount();
uint16_t infoScreenVisibleLines();

// Pantalla completa empezando en la linea topLine
void infoScreenDraw(LGFX_Sprite& canvas, uint16_t topLine, bool isOnline);
