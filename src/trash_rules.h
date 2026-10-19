#pragma once

// Logica pura de la papelera, sin dependencias de Arduino: se compila
// tambien en el entorno native para los tests (pio test -e native). Mover
// ficheros es cosa de storage; la pantalla, de trash.

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>

#include "button_rules.h"
#include "message_rules.h"
#include "utils.h"

// ----- Rutas --------------------------------------------------------------------

// Igual que DIR_MESSAGES y DIR_AUDIOS de config.h (aqui no se puede incluir)
constexpr const char* ACTIVE_MESSAGES_DIR = "/mensajes";
constexpr const char* ACTIVE_AUDIOS_DIR = "/audios";
constexpr const char* TRASH_DIR = "/papelera";
constexpr const char* TRASH_MESSAGES_DIR = "/papelera/mensajes";
constexpr const char* TRASH_AUDIOS_DIR = "/papelera/audios";
// "/papelera/mensajes/" + 40 caracteres de id + ".json" + terminador = 65
constexpr size_t TRASH_PATH_SIZE = 72;

enum class TrashDirection : uint8_t { ToTrash, FromTrash };

// De donde a donde se mueven el JSON y el MP3 de un mensaje. El MP3 puede no
// existir (mensajes de texto): lo comprueba quien mueve.
struct TrashPaths {
  char fromJson[TRASH_PATH_SIZE];
  char toJson[TRASH_PATH_SIZE];
  char fromAudio[TRASH_PATH_SIZE];
  char toAudio[TRASH_PATH_SIZE];
};

inline void buildMessageFilePath(const char* dir, const char* id, const char* ext, char* out, size_t outSize) {
  snprintf(out, outSize, "%s/%s%s", dir, id, ext);
}

// false si el id no vale como nombre de fichero (no se toca nada)
inline bool trashPathsFor(const char* id, TrashDirection direction, TrashPaths& out) {
  if (!isValidMessageId(id, MSG_SUMMARY_ID_MAX)) {
    return false;
  }
  const bool isToTrash = direction == TrashDirection::ToTrash;
  const char* fromMsg = isToTrash ? ACTIVE_MESSAGES_DIR : TRASH_MESSAGES_DIR;
  const char* toMsg = isToTrash ? TRASH_MESSAGES_DIR : ACTIVE_MESSAGES_DIR;
  const char* fromAudio = isToTrash ? ACTIVE_AUDIOS_DIR : TRASH_AUDIOS_DIR;
  const char* toAudio = isToTrash ? TRASH_AUDIOS_DIR : ACTIVE_AUDIOS_DIR;
  buildMessageFilePath(fromMsg, id, ".json", out.fromJson, sizeof(out.fromJson));
  buildMessageFilePath(toMsg, id, ".json", out.toJson, sizeof(out.toJson));
  buildMessageFilePath(fromAudio, id, ".mp3", out.fromAudio, sizeof(out.fromAudio));
  buildMessageFilePath(toAudio, id, ".mp3", out.toAudio, sizeof(out.toAudio));
  return true;
}

// ----- Filas de la papelera -------------------------------------------------------------

// Los que estan en la cola de borrado remoto no salen en "Mensajes en
// papelera": ya se pidio vaciarlos. isQueued(id) dice si un id esta en la
// cola. Devuelve cuantos indices escribio.
template <typename IsQueuedFn>
uint16_t trashVisibleIndex(const MessageSummary* items, size_t count, IsQueuedFn isQueued, uint16_t* out,
                           size_t outCapacity) {
  uint16_t written = 0;
  for (size_t i = 0; i < count && written < outCapacity; ++i) {
    if (!isQueued(items[i].id)) {
      out[written++] = static_cast<uint16_t>(i);
    }
  }
  return written;
}

// ----- Navegacion -----------------------------------------------------------------

// Menu (trash_menu_rules.h), "Ver mensajes" y "Seleccionar" (multi_select_rules.h)
enum class TrashScreen : uint8_t { MENU, LIST, SELECT };

enum class TrashListAction : uint8_t { None, Open, Back };

struct TrashListStep {
  MessageListModel model;
  TrashListAction action;
};

// "Ver mensajes": 1/2 mueven, 3 abre, 4 vuelve al menu de la papelera.
// Restaurar es mantener 3 (UI_LONG_HOLD_MS); la pulsacion larga normal no hace nada.
inline TrashListStep trashListOnButton(const MessageListModel& m, const ButtonEvent& ev, uint16_t count,
                                       uint16_t visible) {
  if (ev.longPress) {
    return {m, TrashListAction::None};
  }
  const MessageListStep step = messageListOnButton(m, ev, count, visible);
  switch (step.exit) {
    case MessageListExit::Open:
      return {step.model, TrashListAction::Open};
    case MessageListExit::Back:
      return {step.model, TrashListAction::Back};
    case MessageListExit::None:
      break;
  }
  return {step.model, TrashListAction::None};
}
