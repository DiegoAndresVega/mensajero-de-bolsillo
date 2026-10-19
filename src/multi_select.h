#pragma once

#include <cstdint>

#include "button_rules.h"
#include "delete_flow.h"
#include "display.h"
#include "message_rules.h"

// Lista con casillas compartida por Ajustes > Borrado multiple (mensajes
// activos: se mueven a la papelera) y Papelera > Seleccionar (se borran para
// siempre, via la cola de borrado remoto). La fuente decide de donde salen
// las filas y que hace el borrado final. La logica pura esta en
// multi_select_rules.h. Solo lo usa la tarea de UI.

enum class MultiSelectSource : uint8_t {
  Messages,  // lista de mensajes (message_list), sin filtro de favoritos
  Trash      // mensajes visibles de la papelera (trash)
};

// Sin casillas marcadas, arriba del todo. Llamar al entrar o tras recargar
// las filas de la fuente.
void multiSelectOpen(MultiSelectSource source);
MultiSelectSource multiSelectSource();

// Pulsaciones cortas: 1/2 mueven, 3 marca, 4 sale (con dialogo si hay marcadas)
FlowNav multiSelectOnButton(const ButtonEvent& ev);
// Mantener UI_LONG_HOLD_MS: 3 abre el mensaje, 4 pide borrar los marcados
FlowNav multiSelectOnHold(Button button);

// Fila seleccionada, para abrirla en el visor (la seleccion se conserva)
MessageSummary multiSelectCurrent();
uint16_t multiSelectCheckedCount();
// Llama a fn con el id de cada fila marcada, de la ultima a la primera: asi
// quitar una de la fuente no mueve las que quedan por recorrer
void multiSelectForEachChecked(void (*fn)(const char* id));
// A donde se vuelve al salir: Ajustes o el menu de la papelera
FlowNav multiSelectExitNav();

uint32_t multiSelectRevision();
void multiSelectDraw(LGFX_Sprite& canvas, bool isOnline);
