#pragma once

#include <cstdint>

#include "button_rules.h"
#include "delete_flow.h"
#include "display.h"
#include "message_rules.h"

// Papelera en la UI: cache de los mensajes de /papelera/mensajes (en PSRAM,
// como la lista) y sus pantallas: el menu (trash_menu), "Ver mensajes" y
// "Seleccionar" (multi_select). Mover ficheros es cosa de la tarea de
// storage; borrar en el servidor, de deletion_queue. La navegacion es logica
// pura (trash_menu_rules.h y trash_rules.h). Solo lo usa la tarea de UI.

// Lee la papelera de la SD (bloquea un momento) y abre su menu. Se entra
// desde el menu principal y su 4 vuelve a el.
void trashScreenEnter();
// Vuelve al menu (tras vaciar, cancelar la cola o salir de Seleccionar)
void trashShowMenu();
// Si la tarea de red ha borrado algo desde la ultima lectura, vuelve a leer
void trashRefreshIfStale();

FlowNav trashScreenOnButton(const ButtonEvent& ev);
// Mantener un boton UI_LONG_HOLD_MS: en "Ver mensajes" 3 pide restaurar; en
// "Seleccionar" lo de multi_select
FlowNav trashScreenOnHold(Button button);
// Mensaje seleccionado en "Ver mensajes" (para abrirlo en el visor)
MessageSummary trashSelected();

// Mensajes que se ven (los que no estan en la cola de borrado)
uint16_t trashCount();
MessageSummary trashAt(uint16_t i);

// Acciones de los dialogos (delete_flow)
void trashRestore(const char* id);
// Pone en la cola todo lo que se ve. Devuelve cuantos.
uint16_t trashQueueAllVisible();
// Las filas visibles cambian al cambiar la cola
void trashRebuild();

// Sube con cada cambio de lo que se ve
uint32_t trashRevision();
void trashScreenDraw(LGFX_Sprite& canvas, bool isOnline);
