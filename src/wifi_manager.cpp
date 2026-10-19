#include "wifi_manager.h"

#include <WiFi.h>

#include "log.h"
#include "state.h"
#include "utils.h"

std::atomic<bool> wifiConnected{false};

namespace {
constexpr const char* TAG = "WIFI";
constexpr uint32_t CONNECT_POLL_MS = 100;
constexpr uint32_t SIM_CONNECT_DELAY_MS = 500;
// Antes de este anio el reloj no ha pasado por NTP (arranca en 1970)
constexpr int MIN_VALID_YEAR = 2024;
constexpr int TM_BASE_YEAR = 1900;

const AppConfig* cfg = nullptr;
bool isNtpStarted = false;
// Indice de la ultima red que conecto; solo lo toca la tarea de red
uint8_t lastGoodIndex = WIFI_NO_LAST_GOOD;

void onWifiEvent(arduino_event_id_t event) {
  if (event == ARDUINO_EVENT_WIFI_STA_GOT_IP) {
    wifiConnected = true;
  } else if (event == ARDUINO_EVENT_WIFI_STA_DISCONNECTED || event == ARDUINO_EVENT_WIFI_STA_LOST_IP) {
    wifiConnected = false;
  }
}

// Un intento con una red; bloquea hasta conectar o agotar WIFI_TRY_TIMEOUT_MS
bool tryNetwork(const WifiNetwork& network, uint8_t index) {
  logMsg(TAG, "conectando a red %u \"%s\"...", static_cast<unsigned>(index + 1), network.ssid);
  WiFi.begin(network.ssid, network.password);
  const uint32_t start = millis();
  while (!wifiConnected && millis() - start < WIFI_TRY_TIMEOUT_MS) {
    vTaskDelay(pdMS_TO_TICKS(CONNECT_POLL_MS));
  }
  if (!wifiConnected) {
    logMsg(TAG, "\"%s\": timeout tras %lu ms (status=%d)", network.ssid,
           static_cast<unsigned long>(WIFI_TRY_TIMEOUT_MS), WiFi.status());
    WiFi.disconnect();
    return false;
  }
  logMsg(TAG, "conectado a \"%s\", IP %s, RSSI %d dBm", network.ssid, WiFi.localIP().toString().c_str(),
         WiFi.RSSI());
  return true;
}

void startNtpOnce() {
  if (isNtpStarted) {
    return;
  }
  configTime(0, 0, NTP_SERVER);
  isNtpStarted = true;
}
}  // namespace

void wifiInit(const AppConfig& config) {
  cfg = &config;
  if (cfg->wokwiMode) {
    logMsg(TAG, "wokwi_mode: WiFi simulado");
    return;
  }
  // Antes de WiFi.mode: si no, la tarea arduino_events ya recorre la lista
  // de callbacks mientras onEvent la amplia (std::vector sin mutex en el
  // core) y salta a un puntero invalido (Guru Meditation en el arranque)
  WiFi.onEvent(onWifiEvent);
  WiFi.mode(WIFI_STA);
  // La reconexion la gestiona la tarea de red recorriendo las redes
  WiFi.setAutoReconnect(false);
}

namespace {
// Una vuelta por las redes, empezando por la ultima que conecto; se queda en
// la que conecte
bool connectAny() {
  const uint8_t count = cfg->wifiNetworkCount;
  if (cfg->wokwiMode) {
    // Sin redes tambien: no se bloquea la simulacion
    vTaskDelay(pdMS_TO_TICKS(SIM_CONNECT_DELAY_MS));
    wifiConnected = true;
    logMsg(TAG, "conectado (simulado) a \"%s\"", count > 0 ? cfg->wifiNetworks[0].ssid : "-");
    return true;
  }
  if (count == 0) {
    logMsg(TAG, "sin redes en config.json");
    return false;
  }

  uint8_t index = wifiFirstIndex(lastGoodIndex, count);
  for (uint8_t tried = 0; tried < count; ++tried) {
    if (tryNetwork(cfg->wifiNetworks[index], index)) {
      lastGoodIndex = index;
      startNtpOnce();
      return true;
    }
    index = wifiNextIndex(index, count);
  }
  logMsg(TAG, "ninguna de las %u redes conecta, se reintenta en %lu s", static_cast<unsigned>(count),
         static_cast<unsigned long>(WIFI_ALL_FAILED_DELAY_MS / 1000));
  return false;
}
}  // namespace

bool wifiReconnect() {
  if (!cfg->wokwiMode && WiFi.status() == WL_CONNECTED) {
    WiFi.disconnect();
  }
  // Sin esto, el primer intento veria aun la conexion anterior
  wifiConnected = false;
  return connectAny();
}

String wifiIsoTimeNow() {
  struct tm now;
  if (!getLocalTime(&now, 0) || now.tm_year + TM_BASE_YEAR < MIN_VALID_YEAR) {
    return "";
  }
  char buf[sizeof(SystemState::ultimaSync)];
  strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &now);
  return String(buf);
}
