#pragma once

#include <cstdint>

#include "button_rules.h"
#include "display.h"

// Borrado de mensajes: dialogos de confirmacion (dialog_rules.h) y lo que se
// hace al confirmar (mover a la papelera, restaurar, vaciar, cancelar la
// cola, borrar los marcados de la seleccion multiple). Devuelve a donde tiene
// que ir la UI; los cambios de estado los hacen ui_input y ui_manager. Solo
// lo usa la tarea de UI.

// A donde va la UI tras un boton o al terminar algo
enum class FlowNav : uint8_t {
  None,                // se queda donde esta (repintar si cambio algo)
  ToMessageList,       // lista de mensajes (tras borrar uno)
  ToSettings,          // Ajustes (tras el borrado multiple o al salir de el)
  ToMainMenu,          // menu principal (al salir de la papelera)
  ToTrashMenu,         // menu de la papelera
  OpenTrashMessage,    // abrir el mensaje seleccionado en Papelera > Ver mensajes
  OpenSelectedMessage  // abrir la fila seleccionada de la seleccion multiple
};

// ----- Dialogos -------------------------------------------------------------------

// "Borrar mensaje?" + "Seguro?". Al confirmar va a la papelera.
void deleteFlowAskDeleteOne(const char* id);
// "Restaurar?" del mensaje seleccionado de la papelera
void deleteFlowAskRestore(const char* id);
// "Vaciar papelera?" + "Seguro?"
void deleteFlowAskEmptyTrash();
// "Cancelar cola pendiente?"
void deleteFlowAskCancelQueue();
// Seleccion multiple: "Cancelar seleccion?" (3 sale sin borrar, 4 se queda)
void deleteFlowAskCancelSelection();
// Seleccion multiple: "Borrar N seleccionados?" + "Seguro?". isFromTrash:
// borrar para siempre (cola de borrado remoto); si no, mover a la papelera.
void deleteFlowAskDeleteSelected(uint16_t count, bool isFromTrash);
// Aviso con solo OK; al cerrarlo la UI va a `nav`
void deleteFlowNotice(const char* question, const char* detail, FlowNav nav);

bool deleteFlowIsOpen();
// Cierra el dialogo sin hacer nada (la UI cambia de pantalla por otra causa).
// Un vaciado en marcha sigue en la tarea de red.
void deleteFlowClose();
// Solo con un dialogo abierto: 3 = si, 4 = no
FlowNav deleteFlowOnButton(const ButtonEvent& ev);
// Una vez por fotograma: "Borrando..." pasa al resultado cuando termina la cola
void deleteFlowPoll();
// Sube cada vez que cambia lo que muestra el dialogo
uint32_t deleteFlowRevision();
// Pinta el dialogo encima del canvas (la primera vez oscurece el fondo)
void deleteFlowDraw(LGFX_Sprite& canvas);
