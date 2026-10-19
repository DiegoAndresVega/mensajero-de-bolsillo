#pragma once

#include <ArduinoJson.h>

#include "config.h"

// Estado persistente del sistema (espejo de /estado.json).
// Arrays fijos en vez de String para poder copiarlo entre tareas sin heap.
struct SystemState {
  char ultimoId[MSG_ID_MAX_LEN + 1] = "";
  uint32_t noLeidos = 0;
  // ISO 8601 UTC ("2026-09-22T08:15:00Z"); vacio si aun no hay hora NTP
  char ultimaSync[25] = "";
};

void stateInit();
// Carga /estado.json; si no existe o esta corrupto arranca con estado vacio.
void stateLoad();

// Copia del estado actual; los cambios se hacen con las funciones de abajo,
// que construyen un estado nuevo y lo sustituyen bajo mutex.
SystemState stateSnapshot();
// Llega un lote de `count` mensajes nuevos (ya en la SD); lastId es el ultimo.
// Un solo cambio de ultimoId: la UI los ve aparecer todos a la vez.
SystemState stateWithNewMessages(const char* lastId, uint32_t count);
SystemState stateWithSync(const char* isoTime);
// Se ha abierto en pantalla un mensaje que no se habia visto: noLeidos baja uno (nunca de 0)
SystemState stateWithMessageRead();
// Vuelve de la papelera un mensaje que no se habia abierto: noLeidos sube uno
SystemState stateWithUnreadRestored();

void stateToJson(const SystemState& state, JsonDocument& doc);
