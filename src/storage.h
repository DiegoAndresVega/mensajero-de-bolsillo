#pragma once

#include <ArduinoJson.h>
#include <FS.h>

#include <functional>

#include "config.h"
#include "state.h"

// Acceso a la MicroSD. Todas las funciones son thread-safe (mutex interno),
// pero en regimen normal solo las llama la tarea de almacenamiento; el resto
// de tareas le piden trabajo por cola (ver tasks.h).

bool storageInit();
bool storageIsReady();

// Bloqueo de la SD para quien la lea fuera de este modulo: la libreria de
// audio lee el MP3 directamente del objeto SD global. Es recursivo, asi que
// se puede anidar con las funciones de aqui.
class StorageLock {
 public:
  StorageLock();
  ~StorageLock();
  StorageLock(const StorageLock&) = delete;
  StorageLock& operator=(const StorageLock&) = delete;
};

// true si alguna tarea tiene ahora mismo la SD bloqueada (guardado, descarga
// de audio, lectura de MP3). Solo orientativo: puede cambiar justo despues.
// La UI lo usa para no competir por el bus SPI con una transferencia larga.
bool storageIsBusy();

bool storageReadJson(const char* path, JsonDocument& doc);
// Escritura atomica: fichero temporal + rename, para no dejar JSON a medias
// si se corta la alimentacion.
bool storageWriteJson(const char* path, const JsonDocument& doc);
// Igual que storageWriteJson pero con un escritor arbitrario. Tiene la SD
// bloqueada mientras escribe: para algo largo (una descarga), StorageFileWriter.
bool storageWriteWith(const char* path, const std::function<bool(File&)>& writer);

// Escritura atomica por trozos (descargas): .tmp y rename al final, como
// storageWriteWith, pero la SD solo se bloquea en cada operacion. Entre un
// trozo y otro la usan la tarea de storage, el audio y la UI.
// Si se destruye sin commit(), borra el .tmp.
class StorageFileWriter {
 public:
  explicit StorageFileWriter(const char* path);
  ~StorageFileWriter();
  StorageFileWriter(const StorageFileWriter&) = delete;
  StorageFileWriter& operator=(const StorageFileWriter&) = delete;

  bool isOpen() const;
  bool write(const uint8_t* data, size_t len);
  // Cierra y renombra el .tmp al destino. false si algo falla.
  bool commit();

 private:
  String path_;
  String tmpPath_;
  File file_;
  // commit() ya se llamo
  bool isDone_ = false;
};

// Escribe un bloque de memoria (un fichero embebido en la flash) con
// StorageFileWriter: atomico y sin tener la SD bloqueada todo el rato
bool storageWriteBytes(const char* path, const uint8_t* data, size_t size);

bool readConfig(AppConfig& out);
// true si readConfig encontro /config.json en la SD. Sin el, los ajustes que
// se cambian desde la pantalla no se pueden guardar.
bool storageHasConfigFile();
// Cambian un solo campo de config.json (lectura + escritura atomica). Si no
// hay config.json no crean uno: tendria solo ese campo y dejaria de valer.
bool saveConfigVolume(uint8_t volume);
bool saveConfigNotificationSound(bool isOn);
bool writeState(const SystemState& state);

String messagePath(const char* id);
String audioPath(const char* id);
// Esta en /mensajes o en la papelera
bool messageExists(const char* id);
bool saveMessage(const char* id, const char* json);

// Recorre /mensajes y llama a `fn` con cada mensaje guardado, sin
// "contenido" (la lista no lo necesita). Todo bajo un solo bloqueo de la SD.
// Devuelve cuantos ficheros leyo bien.
size_t storageForEachMessage(const std::function<void(JsonObjectConst)>& fn);

// Llama a `fn` con el nombre (sin carpeta) de cada fichero de `dir`, en el
// orden de la SD. Todo bajo un solo bloqueo. Si `dir` no existe, no hace nada.
void storageForEachFileName(const char* dir, const std::function<void(const char*)>& fn);

// Resultado de storageReadFile
enum class FileReadResult : uint8_t { Ok, Missing, TooBig, Failed };
// Lee `path` entero en `buf` (capacidad `capacity`) y deja su tamano en `size`
FileReadResult storageReadFile(const char* path, uint8_t* buf, size_t capacity, size_t& size);

// Papelera (/papelera/mensajes y /papelera/audios). Mueven el JSON y, si
// existe, el MP3 con rename (atomico en FAT), el MP3 primero y el JSON al
// final. Devuelven false si el mensaje no esta donde se espera o falla la SD.
bool storageMoveToTrash(const char* id);
bool storageRestoreFromTrash(const char* id);
// Borra para siempre el JSON y el MP3 de la papelera
bool storageDeleteFromTrash(const char* id);
// Como storageForEachMessage, pero con los mensajes de la papelera
size_t storageForEachTrashMessage(const std::function<void(JsonObjectConst)>& fn);

enum class MessageFlagResult : uint8_t { Failed, Changed, Unchanged };
// "leido": true
MessageFlagResult markMessageRead(const char* id);
// "favorito": true o false
MessageFlagResult setMessageFavorite(const char* id, bool isFavorite);
