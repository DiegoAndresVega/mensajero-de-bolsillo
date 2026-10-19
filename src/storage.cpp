#include "storage.h"

#include <SD.h>
#include <SPI.h>

#include <algorithm>

#include "log.h"
#include "trash_rules.h"

namespace {
constexpr const char* TAG = "SD";
constexpr size_t CONFIG_MAX_BYTES = 2048;
// Trozo de storageWriteBytes: entre uno y otro la SD queda libre
constexpr size_t WRITE_BYTES_CHUNK = 4096;

SemaphoreHandle_t sdMutex = nullptr;
bool isReady = false;
bool hasConfigFile = false;

String tmpPathFor(const char* path) {
  return String(path) + TMP_SUFFIX;
}

bool ensureDir(const char* dir) {
  if (SD.exists(dir)) {
    return true;
  }
  if (!SD.mkdir(dir)) {
    logMsg(TAG, "no se pudo crear %s", dir);
    return false;
  }
  logMsg(TAG, "creada carpeta %s", dir);
  return true;
}

bool isValidHttpsUrl(const String& url) {
  return url.startsWith("https://") && url.length() > strlen("https://");
}

bool validateConfig(const AppConfig& cfg) {
  if (cfg.wokwiMode) {
    return true;
  }
  if (cfg.wifiNetworkCount == 0) {
    logMsg(TAG, "config.json: ninguna red WiFi valida (wifi_networks)");
    return false;
  }
  if (!isValidHttpsUrl(cfg.apiUrl)) {
    logMsg(TAG, "config.json: api_url debe empezar por https://");
    return false;
  }
  if (cfg.apiToken.isEmpty()) {
    logMsg(TAG, "config.json: falta api_token");
    return false;
  }
  return true;
}

// Copia a cfg las redes de config.json y avisa de lo que no se usa.
// Nunca se imprimen las contrasenas.
void applyWifiNetworks(JsonVariantConst root, AppConfig& cfg) {
  const WifiParseResult wifi = wifiNetworksFromJson(root);
  memcpy(cfg.wifiNetworks, wifi.networks, sizeof(cfg.wifiNetworks));
  cfg.wifiNetworkCount = wifi.count;
  if (wifi.isLegacy && wifi.count > 0) {
    logMsg(TAG, "config.json: formato viejo (wifi_ssid), se usa como unica red");
  }
  if (wifi.ignored > 0) {
    logMsg(TAG, "config.json: %u redes ignoradas (sin ssid o demasiado largas)", static_cast<unsigned>(wifi.ignored));
  }
  if (wifi.extra > 0) {
    logMsg(TAG, "config.json: %u redes de mas, solo se usan las %u primeras", static_cast<unsigned>(wifi.extra),
           static_cast<unsigned>(WIFI_MAX_NETWORKS));
  }
  if (wifi.count == 0) {
    logMsg(TAG, "config.json: sin redes WiFi");
  }
  for (uint8_t i = 0; i < wifi.count; ++i) {
    logMsg(TAG, "config: red %u \"%s\"", static_cast<unsigned>(i + 1), wifi.networks[i].ssid);
  }
}

// Sustituye `path` por el .tmp ya escrito y cerrado. Con la SD bloqueada.
bool replaceWithTmp(const String& tmpPath, const char* path) {
  // FAT no sobrescribe en rename: hay que borrar el destino antes
  if (SD.exists(path) && !SD.remove(path)) {
    logMsg(TAG, "no se pudo reemplazar %s", path);
    return false;
  }
  if (!SD.rename(tmpPath, path)) {
    logMsg(TAG, "rename %s -> %s fallido", tmpPath.c_str(), path);
    return false;
  }
  return true;
}

#ifdef WOKWI_BUILD
// Wokwi en VS Code no permite precargar ficheros en la SD simulada. Sin esto
// no habria forma de arrancar en simulacion. El token es ficticio: en
// wokwi_mode nunca se contacta con la API real.
AppConfig wokwiFallbackConfig() {
  AppConfig cfg;
  cfg.apiUrl = "https://backend.example.com";
  cfg.apiToken = "simulado";
  cfg.wokwiMode = true;
  // Largo y con tildes (UTF-8, como llega del JSON) para probar Info
  cfg.personalMessage =
      "Mensaje de prueba de la pantalla Info. Cada vez que llegue un mensaje nuevo aparecer\xC3\xA1 un "
      "sobre en la pantalla.\n\nPuedes guardar tus favoritos con el bot\xC3\xB3n 3 y escuchar los mensajes "
      "de voz cuando quieras. \xC2\xA1Gracias por probarlo!";
  return cfg;
}
#endif
}  // namespace

StorageLock::StorageLock() {
  if (sdMutex != nullptr) {
    xSemaphoreTakeRecursive(sdMutex, portMAX_DELAY);
  }
}

StorageLock::~StorageLock() {
  if (sdMutex != nullptr) {
    xSemaphoreGiveRecursive(sdMutex);
  }
}

bool storageInit() {
  if (sdMutex == nullptr) {
    sdMutex = xSemaphoreCreateRecursiveMutex();
  }
  StorageLock lock;
  SPI.begin(PIN_SPI_SCK, PIN_SPI_MISO, PIN_SPI_MOSI, PIN_SD_CS);
  // La pantalla comparte el bus: su CS debe estar en alto para no responder
  pinMode(PIN_LCD_CS, OUTPUT);
  digitalWrite(PIN_LCD_CS, HIGH);

  if (!SD.begin(PIN_SD_CS, SPI, SD_SPI_FREQ_HZ)) {
    logMsg(TAG, "fallo al montar la MicroSD (hay tarjeta? FAT32?)");
    isReady = false;
    return false;
  }
  logMsg(TAG, "montada: %llu MB, usados %llu MB", SD.cardSize() / (1024ULL * 1024ULL),
         SD.usedBytes() / (1024ULL * 1024ULL));

  isReady = ensureDir(DIR_MESSAGES) && ensureDir(DIR_AUDIOS) && ensureDir(DIR_SOUNDS) && ensureDir(TRASH_DIR) &&
            ensureDir(TRASH_MESSAGES_DIR) && ensureDir(TRASH_AUDIOS_DIR);
  return isReady;
}

bool storageIsReady() {
  return isReady;
}

bool storageIsBusy() {
  return sdMutex != nullptr && xSemaphoreGetMutexHolder(sdMutex) != nullptr;
}

bool storageReadJson(const char* path, JsonDocument& doc) {
  if (!isReady) {
    return false;
  }
  StorageLock lock;
  // Si se corto la luz entre borrar el original y renombrar, el bueno es el .tmp
  const String tmpPath = tmpPathFor(path);
  const char* source = SD.exists(path) ? path : (SD.exists(tmpPath) ? tmpPath.c_str() : nullptr);
  if (source == nullptr) {
    return false;
  }
  File file = SD.open(source, FILE_READ);
  if (!file) {
    logMsg(TAG, "no se pudo abrir %s", source);
    return false;
  }
  const DeserializationError err = deserializeJson(doc, file);
  file.close();
  if (err) {
    logMsg(TAG, "JSON invalido en %s: %s", source, err.c_str());
    return false;
  }
  return true;
}

bool storageWriteWith(const char* path, const std::function<bool(File&)>& writer) {
  if (!isReady) {
    logMsg(TAG, "SD no disponible, no se escribe %s", path);
    return false;
  }
  StorageLock lock;
  const String tmpPath = tmpPathFor(path);
  File file = SD.open(tmpPath, FILE_WRITE);
  if (!file) {
    logMsg(TAG, "no se pudo crear %s", tmpPath.c_str());
    return false;
  }
  const bool isWritten = writer(file);
  file.close();
  if (!isWritten) {
    SD.remove(tmpPath);
    logMsg(TAG, "escritura fallida de %s", path);
    return false;
  }
  return replaceWithTmp(tmpPath, path);
}

StorageFileWriter::StorageFileWriter(const char* path) : path_(path), tmpPath_(tmpPathFor(path)) {
  if (!isReady) {
    logMsg(TAG, "SD no disponible, no se escribe %s", path);
    return;
  }
  StorageLock lock;
  file_ = SD.open(tmpPath_, FILE_WRITE);
  if (!file_) {
    logMsg(TAG, "no se pudo crear %s", tmpPath_.c_str());
  }
}

StorageFileWriter::~StorageFileWriter() {
  if (isDone_ || !isReady) {
    return;
  }
  StorageLock lock;
  if (file_) {
    file_.close();
  }
  SD.remove(tmpPath_);
}

bool StorageFileWriter::isOpen() const {
  return static_cast<bool>(file_);
}

bool StorageFileWriter::write(const uint8_t* data, size_t len) {
  if (!file_) {
    return false;
  }
  StorageLock lock;
  return file_.write(data, len) == len;
}

bool StorageFileWriter::commit() {
  if (!file_ || isDone_) {
    return false;
  }
  StorageLock lock;
  file_.close();
  // Salga bien o mal, el destructor ya no tiene nada que cerrar ni borrar
  isDone_ = true;
  if (replaceWithTmp(tmpPath_, path_.c_str())) {
    return true;
  }
  SD.remove(tmpPath_);
  return false;
}

bool storageWriteBytes(const char* path, const uint8_t* data, size_t size) {
  StorageFileWriter writer(path);
  if (!writer.isOpen()) {
    return false;
  }
  for (size_t offset = 0; offset < size; offset += WRITE_BYTES_CHUNK) {
    if (!writer.write(data + offset, std::min(WRITE_BYTES_CHUNK, size - offset))) {
      return false;
    }
  }
  return writer.commit();
}

bool storageWriteJson(const char* path, const JsonDocument& doc) {
  return storageWriteWith(path, [&](File& file) { return serializeJson(doc, file) > 0; });
}

bool readConfig(AppConfig& out) {
  JsonDocument doc;
  const bool hasFile = storageReadJson(PATH_CONFIG, doc);
  hasConfigFile = hasFile;
  if (!hasFile) {
#ifdef WOKWI_BUILD
    logMsg(TAG, "sin %s: se usa la config simulada de Wokwi", PATH_CONFIG);
    out = wokwiFallbackConfig();
    return true;
#else
    logMsg(TAG, "no se pudo leer %s", PATH_CONFIG);
    return false;
#endif
  }
  if (doc.overflowed() || measureJson(doc) > CONFIG_MAX_BYTES) {
    logMsg(TAG, "%s demasiado grande", PATH_CONFIG);
    return false;
  }

  AppConfig cfg;
  applyWifiNetworks(doc.as<JsonVariantConst>(), cfg);
  cfg.apiUrl = doc["api_url"] | "";
  cfg.apiToken = doc["api_token"] | "";
  cfg.wokwiMode = doc["wokwi_mode"] | false;
  const int32_t rawVolume = doc["audio_volume"] | static_cast<int32_t>(AUDIO_VOLUME_DEFAULT);
  cfg.audioVolume = clampAudioVolume(rawVolume);
  cfg.notificationSound = doc["notification_sound"] | true;
  cfg.personalMessage = doc["personal_message"] | "";
  if (cfg.audioVolume != rawVolume) {
    logMsg(TAG, "config.json: audio_volume %ld fuera de 0-%u, se usa %u", static_cast<long>(rawVolume),
           static_cast<unsigned>(AUDIO_VOLUME_MAX), static_cast<unsigned>(cfg.audioVolume));
  }
  while (cfg.apiUrl.endsWith("/")) {
    cfg.apiUrl.remove(cfg.apiUrl.length() - 1);
  }

  if (!validateConfig(cfg)) {
    return false;
  }
  out = cfg;
  // Nunca se imprime el token ni la contrasena
  logMsg(TAG, "config: redes=%u api=%s wokwi_mode=%s volumen=%u sonido_notif=%s",
         static_cast<unsigned>(cfg.wifiNetworkCount), cfg.apiUrl.c_str(), cfg.wokwiMode ? "si" : "no",
         static_cast<unsigned>(cfg.audioVolume), cfg.notificationSound ? "si" : "no");
  return true;
}

bool storageHasConfigFile() {
  return hasConfigFile;
}

namespace {
// Cambia un campo de config.json. Un solo bloqueo para lectura y escritura:
// nadie cambia el fichero en medio.
template <typename T>
bool saveConfigField(const char* key, T value) {
  StorageLock lock;
  JsonDocument doc;
  if (!storageReadJson(PATH_CONFIG, doc)) {
    logMsg(TAG, "sin %s: %s no se guarda", PATH_CONFIG, key);
    return false;
  }
  doc[key] = value;
  return storageWriteJson(PATH_CONFIG, doc);
}
}  // namespace

bool saveConfigVolume(uint8_t volume) {
  return saveConfigField("audio_volume", volume);
}

bool saveConfigNotificationSound(bool isOn) {
  return saveConfigField("notification_sound", isOn);
}

bool writeState(const SystemState& state) {
  JsonDocument doc;
  stateToJson(state, doc);
  return storageWriteJson(PATH_STATE, doc);
}

String messagePath(const char* id) {
  return String(DIR_MESSAGES) + "/" + id + ".json";
}

String audioPath(const char* id) {
  return String(DIR_AUDIOS) + "/" + id + ".mp3";
}

namespace {
// El mensaje esta en la papelera
bool isInTrash(const char* id) {
  TrashPaths paths{};
  if (!isReady || !trashPathsFor(id, TrashDirection::FromTrash, paths)) {
    return false;
  }
  StorageLock lock;
  return SD.exists(paths.fromJson);
}
}  // namespace

bool messageExists(const char* id) {
  if (!isReady) {
    return false;
  }
  StorageLock lock;
  // En la papelera tambien cuenta: si el servidor lo reenvia no se duplica
  return SD.exists(messagePath(id)) || isInTrash(id);
}

bool saveMessage(const char* id, const char* json) {
  const String path = messagePath(id);
  return storageWriteWith(path.c_str(), [&](File& file) {
    const size_t len = strlen(json);
    return file.write(reinterpret_cast<const uint8_t*>(json), len) == len;
  });
}

namespace {
constexpr const char* JSON_EXT = ".json";

bool endsWith(const char* s, const char* suffix) {
  const size_t len = strlen(s);
  const size_t suffixLen = strlen(suffix);
  return len >= suffixLen && strcmp(s + len - suffixLen, suffix) == 0;
}

// Fichero de `dir` que hay que leer: los .json, y un .json.tmp solo si su
// .json no existe (se corto la luz entre borrar el original y renombrar)
bool isMessageFileToRead(const char* dir, const char* name) {
  if (endsWith(name, JSON_EXT)) {
    return true;
  }
  if (!endsWith(name, TMP_SUFFIX)) {
    return false;
  }
  String original = String(dir) + "/" + name;
  original.remove(original.length() - strlen(TMP_SUFFIX));
  return endsWith(original.c_str(), JSON_EXT) && !SD.exists(original);
}

size_t forEachMessageIn(const char* dirPath, const std::function<void(JsonObjectConst)>& fn) {
  if (!isReady) {
    return 0;
  }
  StorageLock lock;
  File dir = SD.open(dirPath);
  if (!dir || !dir.isDirectory()) {
    logMsg(TAG, "no se pudo abrir %s", dirPath);
    return 0;
  }
  JsonDocument filter;
  for (const char* key : {"id", "tipo", "autor", "titulo", "fecha", "leido", "favorito"}) {
    filter[key] = true;
  }
  size_t readCount = 0;
  for (File file = dir.openNextFile(); file; file = dir.openNextFile()) {
    const bool shouldRead = !file.isDirectory() && isMessageFileToRead(dirPath, file.name());
    if (shouldRead) {
      JsonDocument doc;
      const DeserializationError err = deserializeJson(doc, file, DeserializationOption::Filter(filter));
      if (err) {
        logMsg(TAG, "JSON invalido en %s: %s", file.name(), err.c_str());
      } else {
        fn(doc.as<JsonObjectConst>());
        ++readCount;
      }
    }
    file.close();
  }
  dir.close();
  return readCount;
}

// Mueve `from` a `to` si existe. FAT no sobrescribe en rename: se borra antes
// el destino (queda de un intento anterior cortado).
bool moveIfExists(const char* from, const char* to) {
  if (!SD.exists(from)) {
    return true;
  }
  if (SD.exists(to) && !SD.remove(to)) {
    logMsg(TAG, "no se pudo reemplazar %s", to);
    return false;
  }
  if (!SD.rename(from, to)) {
    logMsg(TAG, "rename %s -> %s fallido", from, to);
    return false;
  }
  return true;
}

// El MP3 primero y el JSON al final: el JSON dice donde esta el mensaje, asi
// que si se corta la luz en medio el mensaje sigue entero donde estaba y el
// siguiente intento recoge el MP3 ya movido.
bool moveMessageFiles(const char* id, TrashDirection direction) {
  TrashPaths paths{};
  if (!isReady || !trashPathsFor(id, direction, paths)) {
    return false;
  }
  StorageLock lock;
  if (!SD.exists(paths.fromJson)) {
    logMsg(TAG, "%s no existe", paths.fromJson);
    return false;
  }
  return moveIfExists(paths.fromAudio, paths.toAudio) && moveIfExists(paths.fromJson, paths.toJson);
}
}  // namespace

size_t storageForEachMessage(const std::function<void(JsonObjectConst)>& fn) {
  return forEachMessageIn(DIR_MESSAGES, fn);
}

void storageForEachFileName(const char* dir, const std::function<void(const char*)>& fn) {
  if (!isReady) {
    return;
  }
  StorageLock lock;
  File folder = SD.open(dir);
  if (!folder || !folder.isDirectory()) {
    return;
  }
  for (File file = folder.openNextFile(); file; file = folder.openNextFile()) {
    if (!file.isDirectory()) {
      fn(file.name());
    }
    file.close();
  }
  folder.close();
}

FileReadResult storageReadFile(const char* path, uint8_t* buf, size_t capacity, size_t& size) {
  size = 0;
  if (!isReady) {
    return FileReadResult::Failed;
  }
  StorageLock lock;
  File file = SD.open(path, FILE_READ);
  if (!file) {
    return FileReadResult::Missing;
  }
  const size_t fileSize = file.size();
  if (fileSize > capacity) {
    file.close();
    return FileReadResult::TooBig;
  }
  const size_t readSize = file.read(buf, fileSize);
  file.close();
  if (readSize != fileSize) {
    logMsg(TAG, "lectura incompleta de %s", path);
    return FileReadResult::Failed;
  }
  size = readSize;
  return FileReadResult::Ok;
}

size_t storageForEachTrashMessage(const std::function<void(JsonObjectConst)>& fn) {
  return forEachMessageIn(TRASH_MESSAGES_DIR, fn);
}

bool storageMoveToTrash(const char* id) {
  return moveMessageFiles(id, TrashDirection::ToTrash);
}

bool storageRestoreFromTrash(const char* id) {
  return moveMessageFiles(id, TrashDirection::FromTrash);
}

bool storageDeleteFromTrash(const char* id) {
  TrashPaths paths{};
  if (!isReady || !trashPathsFor(id, TrashDirection::FromTrash, paths)) {
    return false;
  }
  StorageLock lock;
  // El MP3 primero: si se corta la luz, el JSON sigue ahi y se reintenta
  if (SD.exists(paths.fromAudio) && !SD.remove(paths.fromAudio)) {
    logMsg(TAG, "no se pudo borrar %s", paths.fromAudio);
    return false;
  }
  if (SD.exists(paths.fromJson) && !SD.remove(paths.fromJson)) {
    logMsg(TAG, "no se pudo borrar %s", paths.fromJson);
    return false;
  }
  return true;
}

namespace {
// Pone el campo booleano `key` a `value` en /mensajes/{id}.json (escritura
// atomica). Si ya tenia ese valor no escribe nada. Un campo que no existe
// cuenta como false ("favorito" en mensajes viejos).
MessageFlagResult setMessageFlag(const char* id, const char* key, bool value) {
  const String path = messagePath(id);
  // Un solo bloqueo para lectura y escritura: nadie cambia el fichero en medio
  StorageLock lock;
  JsonDocument doc;
  if (!storageReadJson(path.c_str(), doc)) {
    return MessageFlagResult::Failed;
  }
  if ((doc[key] | false) == value) {
    return MessageFlagResult::Unchanged;
  }
  doc[key] = value;
  return storageWriteJson(path.c_str(), doc) ? MessageFlagResult::Changed : MessageFlagResult::Failed;
}
}  // namespace

MessageFlagResult markMessageRead(const char* id) {
  return setMessageFlag(id, "leido", true);
}

MessageFlagResult setMessageFavorite(const char* id, bool isFavorite) {
  return setMessageFlag(id, "favorito", isFavorite);
}
