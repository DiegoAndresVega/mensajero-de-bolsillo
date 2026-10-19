#include "state.h"

#include "log.h"
#include "message_rules.h"
#include "storage.h"

namespace {
constexpr const char* TAG = "STATE";

SystemState current;
SemaphoreHandle_t stateMutex = nullptr;

void copyField(char* dest, size_t destSize, const char* src) {
  strlcpy(dest, src != nullptr ? src : "", destSize);
}

SystemState stateFromJson(const JsonDocument& doc) {
  SystemState state;
  copyField(state.ultimoId, sizeof(state.ultimoId), doc["ultimo_id"] | "");
  state.noLeidos = doc["no_leidos"] | 0u;
  copyField(state.ultimaSync, sizeof(state.ultimaSync), doc["ultima_sync"] | "");
  return state;
}

// Aplica `transform` sobre una copia y la publica como estado actual.
template <typename Fn>
SystemState replaceState(Fn transform) {
  xSemaphoreTake(stateMutex, portMAX_DELAY);
  SystemState next = current;
  transform(next);
  current = next;
  xSemaphoreGive(stateMutex);
  return next;
}
}  // namespace

void stateInit() {
  stateMutex = xSemaphoreCreateMutex();
}

void stateLoad() {
  JsonDocument doc;
  if (!storageReadJson(PATH_STATE, doc)) {
    logMsg(TAG, "sin %s valido, se empieza con estado vacio", PATH_STATE);
    return;
  }
  const SystemState loaded = stateFromJson(doc);
  replaceState([&](SystemState& s) { s = loaded; });
  logMsg(TAG, "cargado: ultimoId=%s noLeidos=%lu ultimaSync=%s", loaded.ultimoId,
         static_cast<unsigned long>(loaded.noLeidos), loaded.ultimaSync);
}

SystemState stateSnapshot() {
  xSemaphoreTake(stateMutex, portMAX_DELAY);
  const SystemState copy = current;
  xSemaphoreGive(stateMutex);
  return copy;
}

SystemState stateWithNewMessages(const char* lastId, uint32_t count) {
  return replaceState([&](SystemState& s) {
    copyField(s.ultimoId, sizeof(s.ultimoId), lastId);
    s.noLeidos += count;
  });
}

SystemState stateWithSync(const char* isoTime) {
  return replaceState([&](SystemState& s) { copyField(s.ultimaSync, sizeof(s.ultimaSync), isoTime); });
}

SystemState stateWithMessageRead() {
  return replaceState([](SystemState& s) { s.noLeidos = unreadAfterRead(s.noLeidos); });
}

SystemState stateWithUnreadRestored() {
  return replaceState([](SystemState& s) { s.noLeidos += 1; });
}

void stateToJson(const SystemState& state, JsonDocument& doc) {
  doc["ultimo_id"] = state.ultimoId;
  doc["no_leidos"] = state.noLeidos;
  doc["ultima_sync"] = state.ultimaSync;
}

