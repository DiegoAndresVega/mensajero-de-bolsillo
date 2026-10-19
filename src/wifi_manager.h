#pragma once

#include <atomic>

#include "config.h"

// true mientras haya IP. Lo actualizan los eventos de WiFi (o la simulacion
// en wokwi_mode), asi que refleja caidas aunque nadie este haciendo peticiones.
extern std::atomic<bool> wifiConnected;

void wifiInit(const AppConfig& config);
// Corta la conexion actual (si la hay) y da una vuelta por las redes de
// config.json, en orden, WIFI_TRY_TIMEOUT_MS cada una. Se queda en la primera
// que conecte. Devuelve false si fallan todas: el llamante espera
// WIFI_ALL_FAILED_DELAY_MS y vuelve a llamar.
bool wifiReconnect();
// Hora UTC en ISO 8601 si NTP ya sincronizo; cadena vacia si no.
String wifiIsoTimeNow();
