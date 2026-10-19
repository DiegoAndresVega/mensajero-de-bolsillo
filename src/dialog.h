#pragma once

#include "display.h"

// Dialogo modal: oscurece lo que haya en el canvas y pinta encima una caja
// con una pregunta, una linea opcional y la ayuda de los botones. La logica
// (que boton hace que) esta en dialog_rules.h. Solo lo usa la tarea de UI.

struct DialogText {
  const char* question;
  // Segunda linea mas pequena; nullptr si no hay
  const char* detail;
  // Ayuda de los botones ("4 = No   3 = Si"); nullptr en "Borrando..."
  const char* buttons;
};

// isFirstDraw: oscurece el fondo (solo una vez: oscurecer dos veces lo
// dejaria negro). En los cambios de texto se repinta solo la caja.
void dialogDraw(LGFX_Sprite& canvas, const DialogText& text, bool isFirstDraw);
