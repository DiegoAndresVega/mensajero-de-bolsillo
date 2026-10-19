#pragma once

#include "button_rules.h"

// Los 4 pulsadores. Una tarea en el core 1 lee los GPIO cada BTN_POLL_MS,
// ButtonDecoder (button_rules.h) quita el rebote y detecta toque, doble toque
// y pulsacion larga, y los eventos van a una cola.

// Configura los pines y arranca la tarea. Devuelve false si no se pudo crear
// la cola o la tarea.
bool buttonsInit();

// Saca un evento de la cola sin esperar. true si habia alguno.
bool buttonsPollEvent(ButtonEvent& out);

// true si el boton esta pulsado ahora mismo (lectura directa del GPIO, sin
// quitar el rebote). Para pulsaciones mas largas que la larga normal:
// mantener un boton UI_LONG_HOLD_MS (borrar, restaurar, seleccion multiple).
bool buttonsIsPressed(Button button);
