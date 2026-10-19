#include "trash.h"

#include <esp_heap_caps.h>

#include <algorithm>

#include "config.h"
#include "deletion_queue.h"
#include "log.h"
#include "message_list.h"
#include "multi_select.h"
#include "storage.h"
#include "tasks.h"
#include "trash_menu.h"
#include "trash_menu_rules.h"
#include "trash_rules.h"

namespace {
constexpr const char* TAG = "PAPELERA";
constexpr const char* LIST_TITLE = "Papelera";
constexpr const char* EMPTY_TEXT = "Papelera vac\xED" "a";

// Cache en PSRAM; se reserva en la primera carga y no se libera
MessageSummary* items = nullptr;
uint16_t itemCount = 0;
// Filas visibles: los que no estan en la cola de borrado
uint16_t rows[MSG_LIST_MAX];
uint16_t rowCount = 0;
TrashScreen screen = TrashScreen::MENU;
uint8_t menuSelected = 0;
MessageListModel list;
uint32_t revision = 0;
// Ejecuciones de la cola al leer la SD: si cambia, algo se borro
uint32_t loadedRunCount = 0;

void rebuildRows() {
  rowCount = items != nullptr ? trashVisibleIndex(items, itemCount, deletionQueueContains, rows, MSG_LIST_MAX) : 0;
  list = messageListClamp(list, rowCount, MSG_ROWS_VISIBLE);
  menuSelected = trashMenuClamp(menuSelected, deletionQueueCount());
  ++revision;
}

void load() {
  if (items == nullptr) {
    items = static_cast<MessageSummary*>(heap_caps_malloc(MSG_LIST_MAX * sizeof(MessageSummary), MALLOC_CAP_SPIRAM));
    if (items == nullptr) {
      logMsg(TAG, "sin PSRAM para la papelera");
      return;
    }
  }
  loadedRunCount = deletionQueueRunCount();
  size_t count = 0;
  storageForEachTrashMessage([&count](JsonObjectConst msg) {
    const MessageSummary summary = messageSummaryFromJson(msg);
    if (isValidMessageId(summary.id, MSG_ID_MAX_LEN)) {
      count = messageListInsert(items, count, MSG_LIST_MAX, summary);
    }
  });
  std::sort(items, items + count, compareMessages);
  itemCount = static_cast<uint16_t>(count);
  rebuildRows();
  logMsg(TAG, "%u en la papelera, %u pendientes de borrar", static_cast<unsigned>(itemCount),
         static_cast<unsigned>(deletionQueueCount()));
}

FlowNav onMenuButton(const ButtonEvent& ev) {
  const TrashMenuStep step = trashMenuOnButton(menuSelected, ev, deletionQueueCount());
  menuSelected = step.selected;
  switch (step.action) {
    case TrashMenuAction::None:
      return FlowNav::None;
    case TrashMenuAction::Exit:
      return FlowNav::ToMainMenu;
    case TrashMenuAction::OpenList:
      screen = TrashScreen::LIST;
      list = MessageListModel{};
      return FlowNav::None;
    case TrashMenuAction::OpenSelect:
      screen = TrashScreen::SELECT;
      multiSelectOpen(MultiSelectSource::Trash);
      return FlowNav::None;
    case TrashMenuAction::AskEmpty:
      // Antes, con todo ya en la cola (vaciada sin conexion) salia solo el
      // aviso "Papelera vacia" y no se hacia nada: ahora pregunta dos veces
      // y reintenta la cola
      if (trashEmptyDecision(rowCount, deletionQueueCount()) == TrashEmptyDecision::NothingToEmpty) {
        deleteFlowNotice(EMPTY_TEXT, nullptr, FlowNav::None);
      } else {
        deleteFlowAskEmptyTrash();
      }
      return FlowNav::None;
    case TrashMenuAction::AskCancelQueue:
      deleteFlowAskCancelQueue();
      return FlowNav::None;
  }
  return FlowNav::None;
}

FlowNav onListButton(const ButtonEvent& ev) {
  const TrashListStep step = trashListOnButton(list, ev, rowCount, MSG_ROWS_VISIBLE);
  list = step.model;
  switch (step.action) {
    case TrashListAction::Open:
      return FlowNav::OpenTrashMessage;
    case TrashListAction::Back:
      trashShowMenu();
      return FlowNav::None;
    case TrashListAction::None:
      break;
  }
  return FlowNav::None;
}
}  // namespace

void trashScreenEnter() {
  screen = TrashScreen::MENU;
  menuSelected = 0;
  list = MessageListModel{};
  load();
}

void trashShowMenu() {
  screen = TrashScreen::MENU;
  rebuildRows();
}

void trashRefreshIfStale() {
  if (deletionQueueRunCount() != loadedRunCount) {
    load();
    // Las casillas eran de las filas de antes
    if (screen == TrashScreen::SELECT) {
      multiSelectOpen(MultiSelectSource::Trash);
    }
  }
}

FlowNav trashScreenOnButton(const ButtonEvent& ev) {
  ++revision;
  switch (screen) {
    case TrashScreen::MENU:
      return onMenuButton(ev);
    case TrashScreen::LIST:
      return onListButton(ev);
    case TrashScreen::SELECT:
      return multiSelectOnButton(ev);
  }
  return FlowNav::None;
}

FlowNav trashScreenOnHold(Button button) {
  ++revision;
  if (screen == TrashScreen::SELECT) {
    return multiSelectOnHold(button);
  }
  if (screen == TrashScreen::LIST && button == Button::BTN3_OK && rowCount > 0) {
    deleteFlowAskRestore(trashSelected().id);
  }
  return FlowNav::None;
}

MessageSummary trashSelected() {
  return trashAt(list.selected);
}

uint16_t trashCount() {
  return rowCount;
}

MessageSummary trashAt(uint16_t i) {
  return i < rowCount ? items[rows[i]] : MessageSummary{};
}

// Si estaba en la cola sale de ella: ya no hay que borrarlo en el servidor
void trashRestore(const char* id) {
  tasksRequestRestoreFromTrash(id);
  deletionQueueRemove(id);
  for (uint16_t i = 0; i < itemCount; ++i) {
    if (strcmp(items[i].id, id) == 0) {
      std::copy(items + i + 1, items + itemCount, items + i);
      --itemCount;
      break;
    }
  }
  messageListInvalidate();
  rebuildRows();
  logMsg(TAG, "%s restaurado", id);
}

uint16_t trashQueueAllVisible() {
  uint16_t added = 0;
  for (uint16_t i = 0; i < rowCount; ++i) {
    if (deletionQueueAdd(items[rows[i]].id)) {
      ++added;
    }
  }
  deletionQueueSave();
  rebuildRows();
  return added;
}

void trashRebuild() {
  rebuildRows();
}

uint32_t trashRevision() {
  return revision;
}

void trashScreenDraw(LGFX_Sprite& canvas, bool isOnline) {
  switch (screen) {
    case TrashScreen::MENU:
      trashMenuDraw(canvas, menuSelected, deletionQueueCount(), isOnline);
      return;
    case TrashScreen::SELECT:
      multiSelectDraw(canvas, isOnline);
      return;
    case TrashScreen::LIST: {
      const MessageRows source{LIST_TITLE, rowCount, trashAt, nullptr, EMPTY_TEXT, ListEmptyIcon::SadEye};
      messageListDrawRows(canvas, source, list, isOnline);
      return;
    }
  }
}
