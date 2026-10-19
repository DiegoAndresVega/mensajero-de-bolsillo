#pragma once

#include <ArduinoJson.h>

#include "config.h"

// Cliente del backend (API HTTPS, ver README). En wokwi_mode no toca la red: sirve mensajes
// de prueba y recuerda en RAM cuales se marcaron como leidos.

void apiInit(const AppConfig& config);
// GET /api/mensajes/nuevos. Deja en `out` el JSON ya validado
// (tiene "mensajes" como array).
bool fetchNewMessages(JsonDocument& out);
// POST /api/mensajes/leido con {"id": ...}
bool markAsRead(const char* id);
// GET /api/audio/:id, escrito en `path` de forma atomica.
bool downloadAudio(const char* id, const char* path);
// DELETE /api/mensajes/:id (timeout de 10 s y reintentos como el resto).
// true si el servidor lo borro (200) o ya no lo tenia (404); false ante
// error de red, 401, 5xx... No loguea la URL (lleva el token).
bool apiDeleteMessage(const char* id);
