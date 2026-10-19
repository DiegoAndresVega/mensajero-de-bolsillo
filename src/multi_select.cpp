#include "multi_select.h"

#include "config.h"
#include "message_list.h"
#include "multi_select_rules.h"
#include "trash.h"

namespace {
constexpr const char* MESSAGES_TITLE = "Borrado m\xFAltiple";
constexpr const char* TRASH_TITLE = "Seleccionar";
constexpr const char* MESSAGES_EMPTY = "No hay mensajes";
constexpr const char* TRASH_EMPTY = "Papelera vac\xED" "a";

MultiSelectSource source = MultiSelectSource::Messages;
MessageListModel model;
// Una casilla por fila de la fuente
bool checked[MSG_LIST_MAX];
uint32_t revision = 0;

uint16_t rowCount() {
  return source == MultiSelectSource::Messages ? messageListCount() : trashCount();
}

MessageSummary rowAt(uint16_t i) {
  return source == MultiSelectSource::Messages ? messageListAt(i) : trashAt(i);
}
}  // namespace

void multiSelectOpen(MultiSelectSource from) {
  source = from;
  model = MessageListModel{};
  memset(checked, 0, sizeof(checked));
  ++revision;
}

MultiSelectSource multiSelectSource() {
  return source;
}

FlowNav multiSelectOnButton(const ButtonEvent& ev) {
  const uint16_t count = rowCount();
  const MultiSelectStep step = multiSelectOnButton(model, ev, count, MSG_ROWS_VISIBLE, countSelected(checked, count));
  model = step.model;
  ++revision;
  switch (step.action) {
    case MultiSelectAction::Toggle:
      toggleChecked(checked, count, model.selected);
      return FlowNav::None;
    case MultiSelectAction::AskCancel:
      deleteFlowAskCancelSelection();
      return FlowNav::None;
    case MultiSelectAction::Exit:
      return multiSelectExitNav();
    case MultiSelectAction::None:
    case MultiSelectAction::Open:
    case MultiSelectAction::AskDelete:
      break;
  }
  return FlowNav::None;
}

FlowNav multiSelectOnHold(Button button) {
  const uint16_t count = rowCount();
  const uint16_t selected = countSelected(checked, count);
  switch (multiSelectOnHold(button, count, selected)) {
    case MultiSelectAction::Open:
      return FlowNav::OpenSelectedMessage;
    case MultiSelectAction::AskDelete:
      deleteFlowAskDeleteSelected(selected, source == MultiSelectSource::Trash);
      return FlowNav::None;
    default:
      return FlowNav::None;
  }
}

MessageSummary multiSelectCurrent() {
  return rowAt(model.selected);
}

uint16_t multiSelectCheckedCount() {
  return countSelected(checked, rowCount());
}

void multiSelectForEachChecked(void (*fn)(const char* id)) {
  for (uint16_t i = rowCount(); i > 0; --i) {
    if (checked[i - 1]) {
      fn(rowAt(i - 1).id);
    }
  }
}

FlowNav multiSelectExitNav() {
  return source == MultiSelectSource::Messages ? FlowNav::ToSettings : FlowNav::ToTrashMenu;
}

uint32_t multiSelectRevision() {
  return revision;
}

void multiSelectDraw(LGFX_Sprite& canvas, bool isOnline) {
  const bool isTrash = source == MultiSelectSource::Trash;
  const MessageRows rows{isTrash ? TRASH_TITLE : MESSAGES_TITLE,
                         rowCount(),
                         rowAt,
                         checked,
                         isTrash ? TRASH_EMPTY : MESSAGES_EMPTY,
                         ListEmptyIcon::SadEye};
  messageListDrawRows(canvas, rows, model, isOnline);
}
