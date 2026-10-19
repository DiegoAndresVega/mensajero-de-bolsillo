#pragma once

#include "../display.h"

// FreeSans con Latin-1 (0x20-0xFF): tildes, n con tilde, "¿" y "¡". Mismo
// diseno y tamanos que las FreeSans de LovyanGFX, que solo tienen ASCII.
// Los textos se pasan en Latin-1 (un byte por caracter) y el canvas tiene
// desactivada la decodificacion UTF-8 (ver ui_manager.cpp). Origen,
// licencia y como regenerarlas: src/fonts/README.md
extern const lgfx::GFXfont FreeSans9pt8b;
extern const lgfx::GFXfont FreeSansBold9pt8b;
extern const lgfx::GFXfont FreeSansBold12pt8b;
