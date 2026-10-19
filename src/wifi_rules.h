#pragma once

// Logica pura del WiFi con varias redes: lectura de config.json, orden de
// intentos y espera tras fallar todas. Sin dependencias de Arduino (solo
// ArduinoJson): se compila tambien en el entorno native para los tests.

#include <ArduinoJson.h>

#include <cstddef>
#include <cstdint>
#include <cstring>

#include "utils.h"

constexpr uint8_t WIFI_MAX_NETWORKS = 5;
// Limites del estandar: SSID de 32 bytes; clave WPA de 63 caracteres o 64 en hex
constexpr size_t WIFI_SSID_MAX_LEN = 32;
constexpr size_t WIFI_PASSWORD_MAX_LEN = 64;
// Tiempo maximo de cada red antes de pasar a la siguiente. Con 5 s, una red
// con poca senal (-85 dBm) a veces no llegaba a conectar
constexpr uint32_t WIFI_TRY_TIMEOUT_MS = 8000;
// Si ninguna red conecta, se espera esto y se vuelve a empezar (ver wifiFirstIndex)
constexpr uint32_t WIFI_ALL_FAILED_DELAY_MS = 30000;

struct WifiNetwork {
  char ssid[WIFI_SSID_MAX_LEN + 1];
  char password[WIFI_PASSWORD_MAX_LEN + 1];
};

struct WifiParseResult {
  WifiNetwork networks[WIFI_MAX_NETWORKS];
  uint8_t count;
  // Entradas descartadas: sin ssid, no son objeto o pasan de los limites
  uint8_t ignored;
  // Entradas validas que no caben (mas de WIFI_MAX_NETWORKS)
  uint8_t extra;
  // Se leyo el formato viejo (wifi_ssid + wifi_password)
  bool isLegacy;
};

// Red siguiente con vuelta a la primera. Sin redes, o con un indice fuera de
// rango, devuelve 0.
inline uint8_t wifiNextIndex(uint8_t current, uint8_t count) {
  if (count == 0 || current >= count) {
    return 0;
  }
  return static_cast<uint8_t>((current + 1) % count);
}

// Aun no ha conectado ninguna red desde el arranque
constexpr uint8_t WIFI_NO_LAST_GOOD = UINT8_MAX;

// Red por la que empieza cada vuelta: la ultima que conecto. Asi, si se cae
// la conexion, no se pierden 8 s por cada red de la lista que no esta al
// alcance antes de volver a la buena. Sin ninguna previa, la primera.
inline uint8_t wifiFirstIndex(uint8_t lastGood, uint8_t count) {
  return lastGood < count ? lastGood : 0;
}

// Lo que falta de la espera de WIFI_ALL_FAILED_DELAY_MS desde que fallaron
// todas las redes. 0 si no fallaron o si ya paso.
inline uint32_t wifiAllFailedRemainingMs(bool hasAllFailed, uint32_t allFailedMs, uint32_t nowMs) {
  if (!hasAllFailed || hasElapsed(allFailedMs, nowMs, WIFI_ALL_FAILED_DELAY_MS)) {
    return 0;
  }
  return WIFI_ALL_FAILED_DELAY_MS - (nowMs - allFailedMs);
}

// true mientras dure la espera tras fallar todas: no se vuelve a intentar
// salvo que se pida desde Ajustes > WiFi
inline bool wifiShouldWaitAllFailed(bool hasAllFailed, uint32_t allFailedMs, uint32_t nowMs) {
  return wifiAllFailedRemainingMs(hasAllFailed, allFailedMs, nowMs) > 0;
}

// Lo que puede tardar como mucho una vuelta por todas las redes
inline uint32_t wifiScanMaxMs(uint8_t count) {
  return (count == 0 ? 1 : count) * WIFI_TRY_TIMEOUT_MS;
}

// Copia ssid y password si son validos. Password ausente = red abierta.
inline bool wifiNetworkFrom(JsonVariantConst ssidJson, JsonVariantConst passwordJson, WifiNetwork& out) {
  const char* ssid = ssidJson.as<const char*>();
  const char* password = passwordJson.isNull() ? "" : passwordJson.as<const char*>();
  if (ssid == nullptr || password == nullptr) {
    return false;
  }
  const size_t ssidLen = strlen(ssid);
  if (ssidLen == 0 || ssidLen > WIFI_SSID_MAX_LEN || strlen(password) > WIFI_PASSWORD_MAX_LEN) {
    return false;
  }
  strncpy(out.ssid, ssid, sizeof(out.ssid));
  strncpy(out.password, password, sizeof(out.password));
  return true;
}

// Redes de config.json. "wifi_networks" (array) manda; si no existe se usa
// el formato viejo "wifi_ssid" + "wifi_password" como unica red.
inline WifiParseResult wifiNetworksFromJson(JsonVariantConst root) {
  WifiParseResult result{};
  const JsonVariantConst list = root["wifi_networks"];
  if (!list.is<JsonArrayConst>()) {
    result.isLegacy = true;
    if (wifiNetworkFrom(root["wifi_ssid"], root["wifi_password"], result.networks[0])) {
      result.count = 1;
    }
    return result;
  }
  for (JsonVariantConst entry : list.as<JsonArrayConst>()) {
    WifiNetwork network{};
    if (!entry.is<JsonObjectConst>() || !wifiNetworkFrom(entry["ssid"], entry["password"], network)) {
      ++result.ignored;
    } else if (result.count >= WIFI_MAX_NETWORKS) {
      ++result.extra;
    } else {
      result.networks[result.count++] = network;
    }
  }
  return result;
}
