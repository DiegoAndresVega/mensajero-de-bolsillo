#include "delete_flow.h"

#include "config.h"
#include "deletion_queue.h"
#include "dialog.h"
#include "dialog_rules.h"
#include "log.h"
#include "message_list.h"
#include "multi_select.h"
#include "tasks.h"
#include "trash.h"
#include "wifi_manager.h"

namespace {
constexpr const char* TAG = "BORRAR";
// Textos en Latin-1: \xBF = abrir interrogacion, \xED = i con tilde...
// Cancelar a la izquierda y aceptar a la derecha, como estan los botones
constexpr const char* YES_NO = "4 = No     3 = S\xED";
constexpr const char* OK_ONLY = "3 = OK";
constexpr const char* SURE = "\xBFSeguro?";
constexpr size_t TEXT_MAX = 48;

enum class Ask : uint8_t {
  None,
  DeleteOne,
  DeleteSelected,
  CancelSelection,
  Restore,
  EmptyTrash,
  CancelQueue,
  Notice,
  Progress
};

struct Flow {
  Ask ask = Ask::None;
  DialogModel dialog;
  char id[MSG_SUMMARY_ID_MAX + 1] = "";
  uint16_t manyCount = 0;
  // Seleccion de la papelera: borrar para siempre; si no, mover a la papelera
  bool isFromTrash = false;
  // Aviso final: textos y a donde se va al cerrarlo
  char noticeQuestion[TEXT_MAX] = "";
  char noticeDetail[TEXT_MAX] = "";
  FlowNav noticeNav = FlowNav::None;
  // "Borrando...": cuantas ejecuciones de la cola iban al empezar
  uint32_t runCountAtStart = 0;
  // La proxima vez que se pinte hay que oscurecer el fondo
  bool isFresh = false;
  uint32_t revision = 0;
};

Flow flow;

void open(Ask ask, uint8_t steps) {
  flow.ask = ask;
  flow.dialog = dialogOpen(steps);
  flow.isFresh = true;
  ++flow.revision;
}

// Cierra y muestra un aviso con solo OK (el fondo ya esta oscurecido)
void showNotice(const char* question, const char* detail, FlowNav nav) {
  flow.ask = Ask::Notice;
  flow.dialog = dialogOpen(1);
  strlcpy(flow.noticeQuestion, question, sizeof(flow.noticeQuestion));
  strlcpy(flow.noticeDetail, detail != nullptr ? detail : "", sizeof(flow.noticeDetail));
  flow.noticeNav = nav;
  ++flow.revision;
}

void closeFlow() {
  flow.ask = Ask::None;
  flow.dialog = DialogModel{};
  ++flow.revision;
}

// ----- Lo que se hace al confirmar ----------------------------------------------------

void moveToTrash(const char* id) {
  tasksRequestMoveToTrash(id);
  messageListRemove(id);
}

void queueForDeletion(const char* id) {
  deletionQueueAdd(id);
}

// Lo que haya en la cola se borra en el servidor y luego en la SD. Los
// ficheros no se tocan hasta que el servidor confirme: sin conexion no se
// pierde nada. Al terminar se vuelve al menu de la papelera.
FlowNav startRemoteDeletion() {
  if (!wifiConnected) {
    showNotice("Sin conexi\xF3n.", "Se borrar\xE1 al conectar.", FlowNav::ToTrashMenu);
    return FlowNav::None;
  }
  flow.ask = Ask::Progress;
  flow.dialog = dialogOpen(1);
  flow.runCountAtStart = deletionQueueRunCount();
  ++flow.revision;
  tasksRequestDeletionRun();
  return FlowNav::None;
}

FlowNav emptyTrash() {
  const uint16_t queued = trashQueueAllVisible();
  logMsg(TAG, "vaciar papelera: %u a la cola", static_cast<unsigned>(queued));
  return startRemoteDeletion();
}

// Ajustes > Borrado multiple mueve a la papelera; Papelera > Seleccionar
// borra para siempre (como vaciar, pero solo los marcados)
FlowNav deleteSelected() {
  const uint16_t count = multiSelectCheckedCount();
  if (!flow.isFromTrash) {
    multiSelectForEachChecked(moveToTrash);
    logMsg(TAG, "%u mensajes a la papelera", static_cast<unsigned>(count));
    closeFlow();
    return FlowNav::ToSettings;
  }
  multiSelectForEachChecked(queueForDeletion);
  deletionQueueSave();
  trashRebuild();
  logMsg(TAG, "%u de la papelera a la cola", static_cast<unsigned>(count));
  return startRemoteDeletion();
}

FlowNav onConfirmed(Ask ask) {
  switch (ask) {
    case Ask::DeleteOne:
      logMsg(TAG, "%s a la papelera", flow.id);
      moveToTrash(flow.id);
      closeFlow();
      return FlowNav::ToMessageList;
    case Ask::DeleteSelected:
      return deleteSelected();
    case Ask::CancelSelection: {
      const FlowNav nav = multiSelectExitNav();
      closeFlow();
      return nav;
    }
    case Ask::Restore:
      trashRestore(flow.id);
      closeFlow();
      return FlowNav::None;
    case Ask::EmptyTrash:
      return emptyTrash();
    case Ask::CancelQueue:
      deletionQueueCancel();
      trashRebuild();
      closeFlow();
      return FlowNav::None;
    case Ask::Notice:
    case Ask::Progress:
    case Ask::None:
      break;
  }
  closeFlow();
  return FlowNav::None;
}

// Un "No" deja todo como estaba y se queda en la misma pantalla
FlowNav onCancelled() {
  closeFlow();
  return FlowNav::None;
}

DialogText textOf(const Flow& f, char* buffer, size_t bufferSize) {
  const bool isSecond = f.dialog.step > 0;
  switch (f.ask) {
    case Ask::DeleteOne:
      return {isSecond ? SURE : "\xBF" "Borrar mensaje?", nullptr, YES_NO};
    case Ask::DeleteSelected:
      snprintf(buffer, bufferSize, "\xBF" "Borrar %u seleccionados?", static_cast<unsigned>(f.manyCount));
      if (!isSecond) {
        return {buffer, f.isFromTrash ? "Para siempre" : "Van a la papelera", YES_NO};
      }
      return {SURE, f.isFromTrash ? "Se borran tambi\xE9n del servidor" : nullptr, YES_NO};
    case Ask::CancelSelection:
      return {"\xBF" "Cancelar selecci\xF3n?", "No se borra nada", "4 = Seguir     3 = Salir"};
    case Ask::Restore:
      return {"\xBFRestaurar?", "Vuelve a la lista de mensajes", YES_NO};
    case Ask::EmptyTrash:
      return {isSecond ? SURE : "\xBFVaciar papelera?", isSecond ? "Se borran tambi\xE9n del servidor" : nullptr,
              YES_NO};
    case Ask::CancelQueue:
      return {"\xBF" "Cancelar cola pendiente?", "Siguen en la papelera", YES_NO};
    case Ask::Notice:
      return {f.noticeQuestion, f.noticeDetail[0] != '\0' ? f.noticeDetail : nullptr, OK_ONLY};
    case Ask::Progress:
      return {"Borrando...", "4 = seguir en segundo plano", nullptr};
    case Ask::None:
      break;
  }
  return {"", nullptr, nullptr};
}
}  // namespace

// ----- Dialogos -------------------------------------------------------------------

void deleteFlowAskDeleteOne(const char* id) {
  strlcpy(flow.id, id, sizeof(flow.id));
  open(Ask::DeleteOne, 2);
}

void deleteFlowAskRestore(const char* id) {
  strlcpy(flow.id, id, sizeof(flow.id));
  open(Ask::Restore, 1);
}

void deleteFlowAskEmptyTrash() {
  open(Ask::EmptyTrash, 2);
}

void deleteFlowAskCancelQueue() {
  open(Ask::CancelQueue, 1);
}

void deleteFlowAskCancelSelection() {
  open(Ask::CancelSelection, 1);
}

void deleteFlowAskDeleteSelected(uint16_t count, bool isFromTrash) {
  flow.manyCount = count;
  flow.isFromTrash = isFromTrash;
  open(Ask::DeleteSelected, 2);
}

void deleteFlowNotice(const char* question, const char* detail, FlowNav nav) {
  showNotice(question, detail, nav);
  flow.isFresh = true;
}

bool deleteFlowIsOpen() {
  return flow.ask != Ask::None;
}

void deleteFlowClose() {
  if (deleteFlowIsOpen()) {
    closeFlow();
  }
}

FlowNav deleteFlowOnButton(const ButtonEvent& ev) {
  const Ask ask = flow.ask;
  if (ask == Ask::Progress) {
    if (ev.button == Button::BTN4_BACK && !ev.longPress) {
      closeFlow();
      return FlowNav::ToTrashMenu;
    }
    return FlowNav::None;
  }
  const DialogStep step = dialogOnButton(flow.dialog, ev);
  if (step.result == DialogResult::Pending) {
    if (step.model.step != flow.dialog.step) {
      flow.dialog = step.model;
      ++flow.revision;
    }
    return FlowNav::None;
  }
  if (ask == Ask::Notice) {
    const FlowNav nav = flow.noticeNav;
    closeFlow();
    return nav;
  }
  return step.result == DialogResult::Confirmed ? onConfirmed(ask) : onCancelled();
}

void deleteFlowPoll() {
  if (flow.ask != Ask::Progress || deletionQueueRunCount() == flow.runCountAtStart) {
    return;
  }
  const uint16_t left = deletionQueueCount();
  if (left == 0) {
    showNotice("Borrado terminado", nullptr, FlowNav::ToTrashMenu);
    return;
  }
  char question[TEXT_MAX];
  snprintf(question, sizeof(question), "%u pendientes.", static_cast<unsigned>(left));
  showNotice(question, "Se borrar\xE1n al conectar.", FlowNav::ToTrashMenu);
}

uint32_t deleteFlowRevision() {
  return flow.revision;
}

void deleteFlowDraw(LGFX_Sprite& canvas) {
  char buffer[TEXT_MAX];
  dialogDraw(canvas, textOf(flow, buffer, sizeof(buffer)), flow.isFresh);
  flow.isFresh = false;
}
