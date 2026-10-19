#include "tasks.h"

#include <ArduinoJson.h>
#include <WiFi.h>

#include <atomic>

#include "api_client.h"
#include "audio_player.h"
#include "deletion_queue.h"
#include "log.h"
#include "state.h"
#include "storage.h"
#include "utils.h"
#include "wifi_manager.h"

namespace {

// ----- Tipos de las colas ---------------------------------------------------

enum class StorageOp : uint8_t {
  SaveMessage,
  WriteState,
  SaveVolume,
  MarkRead,
  SetFavorite,
  SaveNotificationSound,
  MoveToTrash,
  RestoreFromTrash,
  DeleteFromTrash,
  SaveDeletionQueue
};

struct StorageRequest {
  StorageOp op;
  char id[MSG_ID_MAX_LEN + 1];
  // Reservado con malloc por quien pide; lo libera la tarea de storage
  char* payload;
  // Volumen (SaveVolume) o 1/0 (SetFavorite, SaveNotificationSound)
  uint8_t value;
  // Tarea a notificar con el resultado; nullptr = no espera respuesta
  TaskHandle_t replyTo;
};

constexpr uint32_t REPLY_OK = 1;
constexpr uint32_t REPLY_FAIL = 2;
constexpr const char* TIPO_AUDIO = "audio";

QueueHandle_t storageQueue = nullptr;
// La UI lo da para que la tarea de red deje de esperar y reconecte
SemaphoreHandle_t netWake = nullptr;
std::atomic<bool> isReconnectRequested{false};

TaskHandle_t netTaskHandle = nullptr;
TaskHandle_t storageTaskHandle = nullptr;
TaskHandle_t debugTaskHandle = nullptr;

// ----- Peticiones a la tarea de storage ------------------------------------

bool requestStorage(StorageOp op, const char* id, char* payload, bool shouldWait, uint8_t value = 0) {
  StorageRequest req{};
  req.op = op;
  strlcpy(req.id, id != nullptr ? id : "", sizeof(req.id));
  req.payload = payload;
  req.value = value;
  req.replyTo = shouldWait ? xTaskGetCurrentTaskHandle() : nullptr;

  if (shouldWait) {
    xTaskNotifyStateClear(nullptr);
  }
  if (xQueueSend(storageQueue, &req, pdMS_TO_TICKS(STORAGE_REPLY_TIMEOUT_MS)) != pdTRUE) {
    logMsg("TASK", "cola de storage llena, se descarta op %u", static_cast<unsigned>(op));
    free(payload);
    return false;
  }
  if (!shouldWait) {
    return true;
  }
  uint32_t reply = 0;
  if (xTaskNotifyWait(0, UINT32_MAX, &reply, pdMS_TO_TICKS(STORAGE_REPLY_TIMEOUT_MS)) != pdTRUE) {
    logMsg("TASK", "timeout esperando a storage");
    return false;
  }
  return reply == REPLY_OK;
}

bool requestSaveMessage(const char* id, const String& json) {
  char* payload = strdup(json.c_str());
  if (payload == nullptr) {
    logMsg("TASK", "sin memoria para guardar %s", id);
    return false;
  }
  return requestStorage(StorageOp::SaveMessage, id, payload, true);
}

void requestWriteState() {
  requestStorage(StorageOp::WriteState, nullptr, nullptr, false);
}

// ----- Tarea de almacenamiento ----------------------------------------------

// Solo la primera vez que se abre: si el JSON ya decia leido, noLeidos no cambia
bool handleMarkRead(const char* id) {
  switch (markMessageRead(id)) {
    case MessageFlagResult::Failed:
      return false;
    case MessageFlagResult::Unchanged:
      return true;
    case MessageFlagResult::Changed:
      break;
  }
  const SystemState next = stateWithMessageRead();
  logMsg("STORE", "%s leido, quedan %lu sin leer", id, static_cast<unsigned long>(next.noLeidos));
  return writeState(next);
}

// "leido" del JSON del mensaje (en /mensajes o en la papelera). Si no se
// puede leer se toma como leido: asi noLeidos no se toca por un error.
bool isReadAt(const String& path) {
  JsonDocument doc;
  return !storageReadJson(path.c_str(), doc) || (doc["leido"] | false);
}

// Un mensaje sin leer que va a la papelera deja de contar en noLeidos; al
// restaurarlo vuelve a contar. El JSON no cambia: se restaura tal cual.
bool handleMoveToTrash(const char* id) {
  const bool wasRead = isReadAt(messagePath(id));
  if (!storageMoveToTrash(id)) {
    return false;
  }
  logMsg("STORE", "%s a la papelera", id);
  return wasRead || writeState(stateWithMessageRead());
}

bool handleRestoreFromTrash(const char* id) {
  if (!storageRestoreFromTrash(id)) {
    return false;
  }
  logMsg("STORE", "%s restaurado", id);
  return isReadAt(messagePath(id)) || writeState(stateWithUnreadRestored());
}

bool handleStorageRequest(const StorageRequest& req) {
  switch (req.op) {
    case StorageOp::SaveMessage:
      return saveMessage(req.id, req.payload);
    case StorageOp::WriteState:
      // Se escribe la foto mas reciente, no la del momento de la peticion
      return writeState(stateSnapshot());
    case StorageOp::SaveVolume:
      return saveConfigVolume(req.value);
    case StorageOp::MarkRead:
      return handleMarkRead(req.id);
    case StorageOp::SaveNotificationSound:
      return saveConfigNotificationSound(req.value != 0);
    case StorageOp::MoveToTrash:
      return handleMoveToTrash(req.id);
    case StorageOp::RestoreFromTrash:
      return handleRestoreFromTrash(req.id);
    case StorageOp::DeleteFromTrash:
      return storageDeleteFromTrash(req.id);
    case StorageOp::SaveDeletionQueue:
      return deletionQueueWriteFile();
    case StorageOp::SetFavorite: {
      const bool isFavorite = req.value != 0;
      const bool isOk = setMessageFavorite(req.id, isFavorite) != MessageFlagResult::Failed;
      if (isOk) {
        logMsg("STORE", "%s %s favoritos", req.id, isFavorite ? "anadido a" : "quitado de");
      }
      return isOk;
    }
  }
  return false;
}

void storageTask(void*) {
  StorageRequest req;
  for (;;) {
    if (xQueueReceive(storageQueue, &req, portMAX_DELAY) != pdTRUE) {
      continue;
    }
    const bool isOk = handleStorageRequest(req);
    if (!isOk) {
      logMsg("STORE", "op %u fallida (id=%s)", static_cast<unsigned>(req.op), req.id);
    }
    free(req.payload);
    if (req.replyTo != nullptr) {
      xTaskNotify(req.replyTo, isOk ? REPLY_OK : REPLY_FAIL, eSetValueWithOverwrite);
    }
  }
}

// ----- Tarea de red ---------------------------------------------------------

// Copia local del mensaje con los campos que necesitara la UI
String buildStoredMessage(JsonObjectConst msg, const char* id, bool isAudio) {
  JsonDocument stored;
  stored.set(msg);
  stored["leido"] = false;
  if (isAudio) {
    stored["audio_path"] = audioPath(id);
  }
  String json;
  serializeJson(stored, json);
  return json;
}

// Guarda un mensaje en la SD (y antes su MP3). No avisa a la UI ni al
// servidor: syncOnce lo hace una sola vez para todo el lote. Devuelve true
// solo si es nuevo y ha quedado guardado.
bool saveIncoming(JsonObjectConst msg) {
  const char* id = msg["id"] | "";
  if (!isValidMessageId(id, MSG_ID_MAX_LEN)) {
    logMsg("NET", "mensaje con id invalido, se ignora");
    return false;
  }
  // Ya guardado en un ciclo anterior pero markAsRead fallo: solo falta avisar
  if (messageExists(id)) {
    logMsg("NET", "%s ya estaba en la SD, reintento de marcar leido", id);
    return false;
  }
  const bool isAudio = strcmp(msg["tipo"] | "", TIPO_AUDIO) == 0;
  if (isAudio && !downloadAudio(id, audioPath(id).c_str())) {
    logMsg("NET", "audio %s no descargado, se reintentara", id);
    return false;
  }
  if (!requestSaveMessage(id, buildStoredMessage(msg, id, isAudio))) {
    logMsg("NET", "no se pudo guardar %s, se reintentara", id);
    return false;
  }
  logMsg("NET", "NUEVO %s [%s] de %s: %s", id, msg["tipo"] | "", msg["autor"] | "", msg["titulo"] | "");
  return true;
}

// Los mensajes de un ciclo entran juntos: primero se guardan todos, luego
// aparecen a la vez (un cambio de estado, un aviso, un sonido) y al final se
// marcan en el servidor. Un mensaje que no se pudo guardar no se marca: el
// servidor lo volvera a enviar en el siguiente ciclo.
void syncMessages(JsonArrayConst messages) {
  const char* lastId = nullptr;
  uint32_t savedCount = 0;
  for (JsonObjectConst msg : messages) {
    if (saveIncoming(msg)) {
      lastId = msg["id"] | "";
      ++savedCount;
    }
  }
  if (savedCount > 0) {
    stateWithNewMessages(lastId, savedCount);
    requestWriteState();
    logMsg("NET", "%lu mensajes nuevos visibles", static_cast<unsigned long>(savedCount));
    // Si suena un mensaje de voz, la tarea de audio lo pausa y la UI lo reanuda
    // al quitar el aviso de la pantalla
    audioNotifyNewMessage();
  }
  // POST leido = "el llavero ya lo tiene": los de este ciclo y los que ya
  // estaban de uno anterior cuyo POST fallo
  for (JsonObjectConst msg : messages) {
    const char* id = msg["id"] | "";
    if (isValidMessageId(id, MSG_ID_MAX_LEN) && messageExists(id)) {
      markAsRead(id);
    }
  }
}

void syncOnce() {
  JsonDocument response;
  if (!fetchNewMessages(response)) {
    return;
  }
  syncMessages(response["mensajes"].as<JsonArrayConst>());
  // Justo al conectar NTP aun no ha puesto el reloj en hora: sin hora se
  // conserva la de la sincronizacion anterior en vez de dejarla vacia
  const String now = wifiIsoTimeNow();
  if (now.isEmpty()) {
    return;
  }
  stateWithSync(now.c_str());
  requestWriteState();
}

void netTask(void* param) {
  const auto* config = static_cast<const AppConfig*>(param);
  wifiInit(*config);
  apiInit(*config);
  // Ultima vez que fallaron todas las redes: hasta WIFI_ALL_FAILED_DELAY_MS
  // despues no se vuelve a intentar (salvo desde Ajustes)
  bool hasAllFailed = false;
  uint32_t allFailedMs = 0;
  for (;;) {
    // Pedido desde Ajustes > WiFi: se intenta aunque ya haya conexion
    const bool isForced = isReconnectRequested.exchange(false);
    bool isOnline = wifiConnected && !isForced;
    // Sin conexion (o caida): se recorren todas las redes desde la primera
    if (!isOnline && (isForced || !wifiShouldWaitAllFailed(hasAllFailed, allFailedMs, millis()))) {
      isOnline = wifiReconnect();
      hasAllFailed = !isOnline;
      allFailedMs = millis();
    }
    if (isOnline) {
      syncOnce();
      deletionQueueRun();
      // Si el WiFi se cayo durante la consulta (senal debil) se reconecta en
      // la siguiente vuelta, sin esperar al siguiente ciclo
      isOnline = wifiConnected;
      if (!isOnline) {
        logMsg("NET", "WiFi caido durante la consulta, se reconecta ya");
      }
    }
    // Espera al siguiente ciclo (o a la siguiente vuelta por las redes), o
    // menos si la UI pide reconectar o vaciar la papelera
    const uint32_t waitMs =
        isOnline ? NET_POLL_INTERVAL_MS : wifiAllFailedRemainingMs(hasAllFailed, allFailedMs, millis());
    xSemaphoreTake(netWake, pdMS_TO_TICKS(waitMs));
  }
}

// ----- Tarea de debug -------------------------------------------------------

UBaseType_t stackFree(TaskHandle_t handle) {
  return handle != nullptr ? uxTaskGetStackHighWaterMark(handle) : 0;
}

void printStatus() {
  const SystemState state = stateSnapshot();
  logMsg("DBG", "wifi=%s rssi=%d sd=%s | ultimoId=%s noLeidos=%lu ultimaSync=%s",
         wifiConnected ? "ok" : "no", wifiConnected ? WiFi.RSSI() : 0, storageIsReady() ? "ok" : "no",
         state.ultimoId[0] != '\0' ? state.ultimoId : "-", static_cast<unsigned long>(state.noLeidos),
         state.ultimaSync[0] != '\0' ? state.ultimaSync : "-");
  logMsg("DBG", "heap=%lu psram=%lu | pila libre net=%u storage=%u debug=%u",
         static_cast<unsigned long>(ESP.getFreeHeap()), static_cast<unsigned long>(ESP.getFreePsram()),
         stackFree(netTaskHandle), stackFree(storageTaskHandle), stackFree(debugTaskHandle));
}

void debugTask(void*) {
  TickType_t lastWake = xTaskGetTickCount();
  for (;;) {
    vTaskDelayUntil(&lastWake, pdMS_TO_TICKS(DEBUG_PRINT_INTERVAL_MS));
    printStatus();
  }
}

}  // namespace

void tasksStart(const AppConfig& config, bool hasNetwork) {
  storageQueue = xQueueCreate(STORAGE_QUEUE_LEN, sizeof(StorageRequest));
  netWake = xSemaphoreCreateBinary();
  if (storageQueue == nullptr || netWake == nullptr) {
    logMsg("TASK", "no se pudieron crear las colas");
    return;
  }

  xTaskCreatePinnedToCore(storageTask, "storage", STACK_STORAGE_TASK, nullptr, PRIO_STORAGE_TASK,
                          &storageTaskHandle, CORE_APP);
  xTaskCreatePinnedToCore(debugTask, "debug", STACK_DEBUG_TASK, nullptr, PRIO_DEBUG_TASK, &debugTaskHandle,
                          CORE_APP);
  if (hasNetwork) {
    // const_cast solo por la firma de FreeRTOS; netTask lo trata como const
    xTaskCreatePinnedToCore(netTask, "net", STACK_NET_TASK, const_cast<AppConfig*>(&config), PRIO_NET_TASK,
                            &netTaskHandle, CORE_NET);
  } else {
    logMsg("TASK", "sin config valida: modo offline, no se crea la tarea de red");
  }
}

void tasksRequestSaveVolume(uint8_t volume) {
  if (storageQueue == nullptr) {
    logMsg("TASK", "tareas sin arrancar: el volumen %u no se guarda", static_cast<unsigned>(volume));
    return;
  }
  requestStorage(StorageOp::SaveVolume, nullptr, nullptr, false, volume);
}

void tasksRequestSaveNotificationSound(bool isOn) {
  if (storageQueue == nullptr) {
    logMsg("TASK", "tareas sin arrancar: el sonido de notificaciones no se guarda");
    return;
  }
  requestStorage(StorageOp::SaveNotificationSound, nullptr, nullptr, false, isOn ? 1 : 0);
}

namespace {
bool requestTrashOp(StorageOp op, const char* id, bool shouldWait) {
  if (storageQueue == nullptr || !isValidMessageId(id, MSG_ID_MAX_LEN)) {
    logMsg("TASK", "no se puede usar la papelera");
    return false;
  }
  return requestStorage(op, id, nullptr, shouldWait);
}
}  // namespace

void tasksRequestMoveToTrash(const char* id) {
  requestTrashOp(StorageOp::MoveToTrash, id, false);
}

void tasksRequestRestoreFromTrash(const char* id) {
  requestTrashOp(StorageOp::RestoreFromTrash, id, false);
}

bool tasksDeleteFromTrash(const char* id) {
  return requestTrashOp(StorageOp::DeleteFromTrash, id, true);
}

void tasksRequestSaveDeletionQueue() {
  if (storageQueue == nullptr) {
    logMsg("TASK", "tareas sin arrancar: la cola de borrado no se guarda");
    return;
  }
  requestStorage(StorageOp::SaveDeletionQueue, nullptr, nullptr, false);
}

void tasksRequestDeletionRun() {
  if (netTaskHandle == nullptr) {
    logMsg("TASK", "sin tarea de red: la cola se ejecutara al conectar");
    return;
  }
  xSemaphoreGive(netWake);
}

void tasksRequestSync() {
  if (netTaskHandle == nullptr) {
    return;
  }
  logMsg("TASK", "consulta de mensajes pedida al encender la pantalla");
  xSemaphoreGive(netWake);
}

void tasksRequestWifiReconnect() {
  if (netTaskHandle == nullptr) {
    logMsg("TASK", "sin tarea de red: no se puede reconectar");
    return;
  }
  logMsg("TASK", "reconexion WiFi pedida desde Ajustes");
  isReconnectRequested = true;
  xSemaphoreGive(netWake);
}

void tasksRequestMarkRead(const char* id) {
  if (storageQueue == nullptr || !isValidMessageId(id, MSG_ID_MAX_LEN)) {
    logMsg("TASK", "no se puede marcar como leido");
    return;
  }
  requestStorage(StorageOp::MarkRead, id, nullptr, false);
}

void tasksRequestSetFavorite(const char* id, bool isFavorite) {
  if (storageQueue == nullptr || !isValidMessageId(id, MSG_ID_MAX_LEN)) {
    logMsg("TASK", "no se puede cambiar el favorito");
    return;
  }
  requestStorage(StorageOp::SetFavorite, id, nullptr, false, isFavorite ? 1 : 0);
}
