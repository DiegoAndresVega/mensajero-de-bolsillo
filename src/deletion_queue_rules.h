#pragma once

// Logica pura de la cola de borrado remoto (ids pendientes de DELETE en el
// servidor), sin dependencias de Arduino: se compila tambien en el entorno
// native para los tests (pio test -e native). Leer y escribir
// /cola_borrado.json y hacer los DELETE es cosa de deletion_queue.

#include <cstddef>
#include <cstdint>
#include <cstring>

#include "message_rules.h"
#include "utils.h"

// Como mucho tantos ids como mensajes caben en la lista (MSG_LIST_MAX)
constexpr size_t DELETION_QUEUE_CAPACITY = 200;
constexpr size_t DELETION_QUEUE_ID_SIZE = MSG_SUMMARY_ID_MAX + 1;

struct DeletionQueueIds {
  char ids[DELETION_QUEUE_CAPACITY][DELETION_QUEUE_ID_SIZE];
  uint16_t count;
};

inline bool queueContains(const DeletionQueueIds& q, const char* id) {
  if (id == nullptr) {
    return false;
  }
  for (uint16_t i = 0; i < q.count; ++i) {
    if (strcmp(q.ids[i], id) == 0) {
      return true;
    }
  }
  return false;
}

// Anade al final. false si el id no es valido o no cabe; si ya estaba, true
// sin repetirlo.
inline bool queueAdd(DeletionQueueIds& q, const char* id) {
  if (!isValidMessageId(id, MSG_SUMMARY_ID_MAX)) {
    return false;
  }
  if (queueContains(q, id)) {
    return true;
  }
  if (q.count >= DELETION_QUEUE_CAPACITY) {
    return false;
  }
  strncpy(q.ids[q.count], id, DELETION_QUEUE_ID_SIZE - 1);
  q.ids[q.count][DELETION_QUEUE_ID_SIZE - 1] = '\0';
  ++q.count;
  return true;
}

// Quita el id manteniendo el orden del resto. false si no estaba.
inline bool queueRemove(DeletionQueueIds& q, const char* id) {
  for (uint16_t i = 0; i < q.count; ++i) {
    if (strcmp(q.ids[i], id) != 0) {
      continue;
    }
    for (uint16_t j = i + 1; j < q.count; ++j) {
      memcpy(q.ids[j - 1], q.ids[j], DELETION_QUEUE_ID_SIZE);
    }
    --q.count;
    return true;
  }
  return false;
}

inline void queueClear(DeletionQueueIds& q) {
  q.count = 0;
}
