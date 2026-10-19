#include "api_client.h"

#include <HTTPClient.h>
#include <WiFiClientSecure.h>

#include <memory>
#include <new>
#include <vector>

#include "certs.h"
#include "download_rules.h"
#include "log.h"
#include "storage.h"
#include "utils.h"
#include "wifi_manager.h"

namespace {
constexpr const char* TAG = "API";
constexpr const char* PATH_NEW = "/api/mensajes/nuevos";
constexpr const char* PATH_READ = "/api/mensajes/leido";
constexpr const char* PATH_AUDIO = "/api/audio/";
// DELETE /api/mensajes/:id
constexpr const char* PATH_MESSAGE = "/api/mensajes/";
constexpr int HTTP_SERVER_ERROR_MIN = 500;
constexpr uint32_t MS_PER_S = 1000;

const AppConfig* cfg = nullptr;

// ----- Utilidades HTTP ------------------------------------------------------

String urlEncode(const String& value) {
  String encoded;
  encoded.reserve(value.length() * 3);
  char buf[3];
  for (size_t i = 0; i < value.length(); ++i) {
    const size_t n = urlEncodeChar(value[i], buf);
    encoded.concat(buf, n);
  }
  return encoded;
}

// La URL lleva el token: no se debe imprimir nunca entera
String buildUrl(const String& path) {
  return cfg->apiUrl + path + "?token=" + urlEncode(cfg->apiToken);
}

void configureClient(WiFiClientSecure& client, HTTPClient& http) {
  client.setCACert(ROOT_CA_GTS_R1);
  client.setHandshakeTimeout(HTTP_TIMEOUT_MS / MS_PER_S);
  http.setConnectTimeout(HTTP_TIMEOUT_MS);
  http.setTimeout(HTTP_TIMEOUT_MS);
  http.setReuse(false);
}

bool isRetryable(int status) {
  // Negativo = error de transporte (DNS, TLS, timeout); 5xx = fallo del servidor.
  // Un 4xx (token malo, id inexistente) no mejora reintentando.
  return status < 0 || status >= HTTP_SERVER_ERROR_MIN;
}

// Ejecuta `request` (devuelve el codigo HTTP) con reintentos y backoff.
template <typename Fn>
int withRetries(const char* what, Fn request) {
  int status = 0;
  for (uint8_t attempt = 0; attempt < HTTP_MAX_RETRIES; ++attempt) {
    status = request();
    // Tras el ultimo intento no se espera: no queda nada que reintentar. Sin
    // WiFi tampoco: cada intento gastaria su timeout y retrasaria la reconexion.
    if (!isRetryable(status) || attempt + 1 == HTTP_MAX_RETRIES || !wifiConnected) {
      return status;
    }
    const uint32_t waitMs = backoffDelayMs(attempt, HTTP_BACKOFF_BASE_MS, HTTP_BACKOFF_MAX_MS);
    logMsg(TAG, "%s: error %d (%s), reintento en %lu ms", what, status,
           status < 0 ? HTTPClient::errorToString(status).c_str() : "servidor",
           static_cast<unsigned long>(waitMs));
    vTaskDelay(pdMS_TO_TICKS(waitMs));
  }
  return status;
}

// ----- Descarga de audio ------------------------------------------------------

enum class BodyResult : uint8_t { Saved, TooBig, WriteFailed, Stalled, Truncated };

// Codigo para withRetries: colgada o cortada se reintenta (error de
// transporte); demasiado grande o fallo de la SD no mejoran reintentando.
int bodyResultCode(BodyResult result, int httpCode) {
  switch (result) {
    case BodyResult::Stalled:
      return HTTPC_ERROR_READ_TIMEOUT;
    case BodyResult::Truncated:
      return HTTPC_ERROR_CONNECTION_LOST;
    default:
      return httpCode;
  }
}

BodyResult bodyResultOf(DownloadStatus status) {
  switch (status) {
    case DownloadStatus::Complete:
      return BodyResult::Saved;
    case DownloadStatus::TooBig:
      return BodyResult::TooBig;
    case DownloadStatus::Stalled:
      return BodyResult::Stalled;
    default:
      return BodyResult::Truncated;
  }
}

// Lee lo que haya (como mucho un trozo) y lo escribe en la SD. Devuelve los
// bytes leidos, o -1 si falla la escritura.
int32_t copyChunk(NetworkClient& stream, StorageFileWriter& writer, uint8_t* buf, const DownloadProgress& p) {
  const int available = stream.available();
  const size_t len =
      downloadChunkLen(available > 0 ? static_cast<size_t>(available) : 0, AUDIO_DOWNLOAD_CHUNK_BYTES, p.received,
                       p.expected);
  if (len == 0) {
    return 0;
  }
  const size_t n = stream.readBytes(buf, len);
  if (n > 0 && !writer.write(buf, n)) {
    return -1;
  }
  return static_cast<int32_t>(n);
}

// Pasa el cuerpo de la respuesta a `path` por trozos. Cede la CPU en cada
// vuelta y la SD solo se bloquea en cada escritura (StorageFileWriter).
// Antes se usaba http.writeToStream(), que da vueltas sin bloquearse mientras
// no llegan datos: el TWDT del core 0 saltaba y la placa se reiniciaba.
BodyResult saveBody(HTTPClient& http, const char* id, const char* path, int32_t expected) {
  StorageFileWriter writer(path);
  if (!writer.isOpen()) {
    return BodyResult::WriteFailed;
  }
  std::unique_ptr<uint8_t[]> buf(new (std::nothrow) uint8_t[AUDIO_DOWNLOAD_CHUNK_BYTES]);
  if (!buf) {
    logMsg(TAG, "audio %s: sin memoria para el buffer", id);
    return BodyResult::Truncated;
  }
  NetworkClient* streamPtr = http.getStreamPtr();
  if (streamPtr == nullptr) {
    return BodyResult::Truncated;
  }
  NetworkClient& stream = *streamPtr;
  const uint32_t startMs = millis();
  DownloadProgress p{0, expected, startMs, startMs, true};
  DownloadStatus status = DownloadStatus::InProgress;
  for (;;) {
    p.nowMs = millis();
    p.isOpen = stream.connected() || stream.available() > 0;
    status = downloadStatus(p, AUDIO_MAX_BYTES, AUDIO_DOWNLOAD_STALL_MS);
    if (status != DownloadStatus::InProgress) {
      break;
    }
    const int32_t n = copyChunk(stream, writer, buf.get(), p);
    if (n < 0) {
      return BodyResult::WriteFailed;
    }
    if (n > 0) {
      p.received += static_cast<size_t>(n);
      p.lastDataMs = millis();
    }
    vTaskDelay(n > 0 ? AUDIO_DOWNLOAD_YIELD_TICKS : pdMS_TO_TICKS(AUDIO_DOWNLOAD_POLL_MS));
  }

  const BodyResult result = bodyResultOf(status);
  logMsg(TAG, "audio %s: %u de %ld bytes en %lu ms (%s)", id, static_cast<unsigned>(p.received),
         static_cast<long>(expected), static_cast<unsigned long>(millis() - startMs),
         result == BodyResult::Saved       ? "completo"
         : result == BodyResult::TooBig    ? "demasiado grande"
         : result == BodyResult::Stalled   ? "sin datos, colgada"
                                           : "cortada");
  if (result == BodyResult::Saved && !writer.commit()) {
    return BodyResult::WriteFailed;
  }
  return result;
}

bool hasMessagesArray(const JsonDocument& doc) {
  return doc["mensajes"].is<JsonArrayConst>();
}

// ----- Simulacion (wokwi_mode) ----------------------------------------------

// Los cuatro ultimos sirven para probar la lista (mas filas de las que caben),
// el scroll del visor con un texto largo y las tildes. Van como escapes JSON
// (\u00e1 = a con tilde): es lo que manda el servidor y llega a la SD en
// UTF-8, no en Latin-1
const char SIM_RESPONSE[] = R"JSON({
  "nuevos": 6,
  "mensajes": [
    {"id": "simTexto001", "tipo": "texto", "autor": "Ana", "titulo": "Buenos d\u00edas",
     "contenido": "Hola, \u00bfqu\u00e9 tal?", "fecha": "2026-09-22T08:15:00Z"},
    {"id": "simAudio002", "tipo": "audio", "autor": "Luis", "titulo": "Canci\u00f3n",
     "fecha": "2026-09-22T09:00:00Z"},
    {"id": "simTexto003", "tipo": "texto", "autor": "Carmen L\u00f3pez",
     "titulo": "Una historia muy larga para ver c\u00f3mo se corta el t\u00edtulo",
     "contenido": "Este es un mensaje largo para probar el scroll del visor y las tildes: \u00e1 \u00e9 \u00ed \u00f3 \u00fa \u00f1 \u00bf \u00a1.\n\nHoy he ido al mercado y hab\u00eda fruta de temporada, as\u00ed que he comprado un poco para hacer mermelada el fin de semana.\n\nSi el texto no cabe en la pantalla, los botones 1 y 2 mueven el scroll arriba y abajo.",
     "fecha": "2026-09-21T18:30:00Z"},
    {"id": "simTexto004", "tipo": "texto", "autor": "Pablo", "titulo": "Noche",
     "contenido": "Descansa", "fecha": "2026-09-20T21:00:00Z"},
    {"id": "simTexto005", "tipo": "texto", "autor": "Sara", "titulo": "Partido",
     "contenido": "\u00a1Hemos ganado 3-1!", "fecha": "2026-08-30T17:45:00Z"},
    {"id": "simTexto006", "tipo": "texto", "autor": "Ana", "titulo": "Feliz a\u00f1o",
     "contenido": "\u00a1Feliz a\u00f1o nuevo!", "fecha": "2025-12-31T23:30:00Z"}
  ]
})JSON";

// Septimo mensaje: solo aparece a partir de SIM_LATE_ARRIVAL_MS desde el
// arranque, para probar el aviso de mensaje nuevo fuera de la primera
// sincronizacion (desde el menu, el visor o con un audio sonando). Llega en
// la primera consulta despues de ese momento (se consulta cada 30 s).
constexpr uint32_t SIM_LATE_ARRIVAL_MS = 90UL * 1000UL;
const char SIM_LATE_MESSAGE[] = R"JSON({"id": "simTexto007", "tipo": "texto", "autor": "Prueba",
  "titulo": "Aviso tard\u00edo", "contenido": "Esto llega tarde para probar el aviso",
  "fecha": "2026-09-24T10:00:00Z"})JSON";

// Corte de conexion simulado para probar la cola de borrado: entre estos
// dos instantes (desde el arranque) el servidor no responde y wifiConnected
// pasa a false. La tarea de red lo "reconecta" en cada ciclo de 30 s y vuelve
// a caer, asi que la UI lo ve casi siempre sin conexion.
constexpr uint32_t SIM_OFFLINE_FROM_MS = 120UL * 1000UL;
constexpr uint32_t SIM_OFFLINE_TO_MS = 240UL * 1000UL;

bool simIsOffline() {
  const uint32_t now = millis();
  if (now < SIM_OFFLINE_FROM_MS || now >= SIM_OFFLINE_TO_MS) {
    return false;
  }
  wifiConnected = false;
  logMsg(TAG, "sin conexion (simulado, hasta los %lu s)", static_cast<unsigned long>(SIM_OFFLINE_TO_MS / 1000));
  return true;
}

std::vector<String> simReadIds;

bool simIsRead(const char* id) {
  for (const String& readId : simReadIds) {
    if (readId == id) {
      return true;
    }
  }
  return false;
}

bool simFetch(JsonDocument& out) {
  if (simIsOffline()) {
    return false;
  }
  JsonDocument all;
  deserializeJson(all, SIM_RESPONSE);
  out.clear();
  JsonArray pending = out["mensajes"].to<JsonArray>();
  if (millis() >= SIM_LATE_ARRIVAL_MS) {
    JsonDocument late;
    deserializeJson(late, SIM_LATE_MESSAGE);
    all["mensajes"].add(late);
  }
  for (JsonObjectConst msg : all["mensajes"].as<JsonArrayConst>()) {
    if (!simIsRead(msg["id"] | "")) {
      pending.add(msg);
    }
  }
  out["nuevos"] = pending.size();
  logMsg(TAG, "GET nuevos (simulado): %u", static_cast<unsigned>(pending.size()));
  return true;
}
}  // namespace

void apiInit(const AppConfig& config) {
  cfg = &config;
}

bool fetchNewMessages(JsonDocument& out) {
  if (cfg->wokwiMode) {
    return simFetch(out);
  }

  String body;
  const int status = withRetries("GET nuevos", [&]() {
    WiFiClientSecure client;
    HTTPClient http;
    configureClient(client, http);
    if (!http.begin(client, buildUrl(PATH_NEW))) {
      return static_cast<int>(HTTPC_ERROR_CONNECTION_REFUSED);
    }
    const int code = http.GET();
    const int size = http.getSize();
    const bool isTooBig = size > 0 && static_cast<size_t>(size) > API_MAX_RESPONSE_BYTES;
    if (code == HTTP_CODE_OK && !isTooBig) {
      body = http.getString();
    }
    http.end();
    return isTooBig ? static_cast<int>(HTTPC_ERROR_TOO_LESS_RAM) : code;
  });

  if (status != HTTP_CODE_OK) {
    logMsg(TAG, "GET nuevos fallido: %d", status);
    return false;
  }
  if (body.length() > API_MAX_RESPONSE_BYTES) {
    logMsg(TAG, "respuesta demasiado grande (%u bytes)", body.length());
    return false;
  }
  const DeserializationError err = deserializeJson(out, body);
  if (err || !hasMessagesArray(out)) {
    logMsg(TAG, "respuesta sin formato esperado: %s", err ? err.c_str() : "falta 'mensajes'");
    return false;
  }
  logMsg(TAG, "GET nuevos: %u", static_cast<unsigned>(out["mensajes"].size()));
  return true;
}

bool markAsRead(const char* id) {
  if (cfg->wokwiMode) {
    simReadIds.push_back(String(id));
    logMsg(TAG, "POST leido %s (simulado)", id);
    return true;
  }

  JsonDocument payload;
  payload["id"] = id;
  String json;
  serializeJson(payload, json);

  const int status = withRetries("POST leido", [&]() {
    WiFiClientSecure client;
    HTTPClient http;
    configureClient(client, http);
    if (!http.begin(client, buildUrl(PATH_READ))) {
      return static_cast<int>(HTTPC_ERROR_CONNECTION_REFUSED);
    }
    http.addHeader("Content-Type", "application/json");
    const int code = http.POST(json);
    http.end();
    return code;
  });

  const bool isOk = status >= HTTP_CODE_OK && status < HTTP_CODE_MULTIPLE_CHOICES;
  logMsg(TAG, "POST leido %s: %d", id, status);
  return isOk;
}

bool downloadAudio(const char* id, const char* path) {
  if (cfg->wokwiMode) {
    logMsg(TAG, "GET audio %s (simulado, no se escribe %s)", id, path);
    return true;
  }

  bool isSaved = false;
  const int status = withRetries("GET audio", [&]() {
    WiFiClientSecure client;
    HTTPClient http;
    configureClient(client, http);
    // HTTP/1.0: el servidor no puede mandar chunks, asi que el cuerpo llega
    // tal cual por getStreamPtr() (el MP3 sin cabeceras de chunk)
    http.useHTTP10(true);
    if (!http.begin(client, buildUrl(String(PATH_AUDIO) + id))) {
      return static_cast<int>(HTTPC_ERROR_CONNECTION_REFUSED);
    }
    const int code = http.GET();
    if (code != HTTP_CODE_OK) {
      http.end();
      return code;
    }
    // -1 si no hay Content-Length
    const BodyResult result = saveBody(http, id, path, http.getSize());
    http.end();
    isSaved = result == BodyResult::Saved;
    return bodyResultCode(result, code);
  });

  logMsg(TAG, "GET audio %s: %d, guardado=%s", id, status, isSaved ? "si" : "no");
  return status == HTTP_CODE_OK && isSaved;
}

bool apiDeleteMessage(const char* id) {
  if (!isValidMessageId(id, MSG_ID_MAX_LEN)) {
    logMsg(TAG, "DELETE con id invalido, se ignora");
    return false;
  }
  if (cfg->wokwiMode) {
    if (simIsOffline()) {
      return false;
    }
    logMsg(TAG, "DELETE %s (simulado)", id);
    return true;
  }

  const int status = withRetries("DELETE", [&]() {
    WiFiClientSecure client;
    HTTPClient http;
    configureClient(client, http);
    if (!http.begin(client, buildUrl(String(PATH_MESSAGE) + id))) {
      return static_cast<int>(HTTPC_ERROR_CONNECTION_REFUSED);
    }
    const int code = http.sendRequest("DELETE");
    http.end();
    return code;
  });
  logMsg(TAG, "DELETE %s: %d", id, status);
  // 404: ya no esta en el servidor (un DELETE anterior llego pero se perdio
  // la respuesta). Para la cola cuenta como hecho: si no, no saldria nunca.
  return status == HTTP_CODE_OK || status == HTTP_CODE_NOT_FOUND;
}
