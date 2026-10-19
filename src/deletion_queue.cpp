#include "deletion_queue.h"

#include <ArduinoJson.h>
#include <SD.h>

#include <atomic>

#include "api_client.h"
#include "config.h"
#include "deletion_queue_rules.h"
#include "log.h"
#include "storage.h"
#include "tasks.h"
#include "wifi_manager.h"

namespace {
constexpr const char* TAG = "COLA";
constexpr const char* KEY_PENDING = "pendientes";

// ~8 KB: en la RAM estatica, no en la pila de ninguna tarea
DeletionQueueIds queue{};
SemaphoreHandle_t queueMutex = nullptr;
std::atomic<uint32_t> runCount{0};

class QueueLock {
 public:
  QueueLock() { xSemaphoreTake(queueMutex, portMAX_DELAY); }
  ~QueueLock() { xSemaphoreGive(queueMutex); }
  QueueLock(const QueueLock&) = delete;
  QueueLock& operator=(const QueueLock&) = delete;
};

void loadFile() {
  JsonDocument doc;
  if (!storageReadJson(TRASH_QUEUE_FILE, doc)) {
    return;
  }
  QueueLock lock;
  for (const char* id : doc[KEY_PENDING].as<JsonArrayConst>()) {
    if (!queueAdd(queue, id)) {
      logMsg(TAG, "id no valido o cola llena, se ignora");
    }
  }
  logMsg(TAG, "%u pendientes de borrar en el servidor", static_cast<unsigned>(queue.count));
}

// Copia el id i en `out`. false si i no existe.
bool idAt(uint16_t i, char* out, size_t outSize) {
  QueueLock lock;
  if (i >= queue.count || outSize == 0) {
    return false;
  }
  strlcpy(out, queue.ids[i], outSize);
  return true;
}
}  // namespace

void deletionQueueInit() {
  queueMutex = xSemaphoreCreateMutex();
  loadFile();
}

uint16_t deletionQueueCount() {
  QueueLock lock;
  return queue.count;
}

bool deletionQueueContains(const char* id) {
  QueueLock lock;
  return queueContains(queue, id);
}

bool deletionQueueAdd(const char* id) {
  QueueLock lock;
  return queueAdd(queue, id);
}

void deletionQueueSave() {
  tasksRequestSaveDeletionQueue();
}

void deletionQueueRemove(const char* id) {
  bool isRemoved = false;
  {
    QueueLock lock;
    isRemoved = queueRemove(queue, id);
  }
  if (isRemoved) {
    logMsg(TAG, "%s sale de la cola", id);
    deletionQueueSave();
  }
}

void deletionQueueCancel() {
  {
    QueueLock lock;
    queueClear(queue);
  }
  logMsg(TAG, "cola cancelada");
  deletionQueueSave();
}

bool deletionQueueWriteFile() {
  JsonDocument doc;
  JsonArray pending = doc[KEY_PENDING].to<JsonArray>();
  {
    QueueLock lock;
    for (uint16_t i = 0; i < queue.count; ++i) {
      pending.add(queue.ids[i]);
    }
  }
  if (pending.size() > 0) {
    return storageWriteJson(TRASH_QUEUE_FILE, doc);
  }
  StorageLock lock;
  return !SD.exists(TRASH_QUEUE_FILE) || SD.remove(TRASH_QUEUE_FILE);
}

void deletionQueueRun() {
  const uint16_t total = deletionQueueCount();
  uint16_t done = 0;
  char id[DELETION_QUEUE_ID_SIZE];
  // Siempre el primero de la cola: los que fallan se saltan con `skipped`
  for (uint16_t skipped = 0; idAt(skipped, id, sizeof(id));) {
    if (!wifiConnected) {
      logMsg(TAG, "sin conexion: se sigue en el siguiente ciclo");
      break;
    }
    if (!apiDeleteMessage(id) || !tasksDeleteFromTrash(id)) {
      ++skipped;
      continue;
    }
    {
      QueueLock lock;
      queueRemove(queue, id);
    }
    deletionQueueSave();
    ++done;
  }
  if (total > 0) {
    logMsg(TAG, "%u de %u borrados, quedan %u", static_cast<unsigned>(done), static_cast<unsigned>(total),
           static_cast<unsigned>(deletionQueueCount()));
  }
  ++runCount;
}

uint32_t deletionQueueRunCount() {
  return runCount.load();
}
