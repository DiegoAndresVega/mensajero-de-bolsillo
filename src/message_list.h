#pragma once

#include <ArduinoJson.h>

#include <cstdint>

#include "display.h"
#include "message_rules.h"

// Lista de mensajes de la SD. Guarda en PSRAM un resumen de cada mensaje
// (sin el contenido), ordenado del mas nuevo al mas antiguo. Se ven todos o
// solo los favoritos: los indices de messageListCount/At son los de las filas
// que se ven. La navegacion es logica pura (messageListOnButton en
// message_rules.h). Solo lo usa la tarea de UI.

// true si la lista no se ha cargado nunca o ha llegado un mensaje desde la
// ultima carga (cambia ultimoId en state). Asi no hay que recorrer la SD
// para saber si algo cambio.
bool messageListNeedsLoad();
// Lee todos los JSON de /mensajes. Bloquea mientras lee la SD.
void messageListLoad();

// Todos los mensajes (Mensajes) o solo los favoritos (Favoritos)
void messageListSetFavoritesOnly(bool isFavoritesOnly);

// Filas que se ven con el filtro actual
uint16_t messageListCount();
// Copia del resumen; i debe ser menor que messageListCount()
MessageSummary messageListAt(uint16_t i);
// Marca el mensaje i como leido en la cache y pide a la tarea de storage que
// lo guarde en su JSON y baje noLeidos. Si ya estaba leido no hace nada.
void messageListMarkRead(uint16_t i);
// Cambia el favorito del mensaje `id` en la cache y pide a la tarea de
// storage que lo guarde en su JSON. Devuelve el valor nuevo (false si no esta
// en la cache). En Favoritos, al desmarcarlo desaparece de las filas.
bool messageListToggleFavorite(const char* id);
// Sube con cada carga o cambio de la cache: sirve para saber si hay que redibujar
uint32_t messageListRevision();

// Quita el mensaje de la cache (se ha movido a la papelera)
void messageListRemove(const char* id);
// La proxima vez que se entre se vuelve a leer la SD (se ha restaurado algo)
void messageListInvalidate();

// Resumen de un JSON de mensaje (sin contenido). Tambien para la papelera.
MessageSummary messageSummaryFromJson(JsonObjectConst msg);

// Pantallas completas: "Cargando..." y la lista (o "No hay mensajes" / "No
// hay favoritos")
void messageListDrawLoading(LGFX_Sprite& canvas, bool isOnline);
void messageListDraw(LGFX_Sprite& canvas, const MessageListModel& model, bool isOnline);

// Misma lista con filas de otra fuente (papelera, borrado multiple)
enum class ListEmptyIcon : uint8_t { SadEye, Heart };
struct MessageRows {
  const char* title;
  uint16_t count;
  // Resumen de la fila i (0 <= i < count)
  MessageSummary (*at)(uint16_t);
  // Casilla de cada fila (borrado multiple); nullptr: punto de no leido
  const bool* checked;
  const char* emptyText;
  ListEmptyIcon emptyIcon;
};
void messageListDrawRows(LGFX_Sprite& canvas, const MessageRows& source, const MessageListModel& model,
                         bool isOnline);
