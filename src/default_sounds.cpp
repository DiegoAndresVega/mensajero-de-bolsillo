#include "default_sounds.h"

#ifdef WOKWI_BUILD
// En Wokwi la notificacion se simula: no hace falta el MP3
void defaultSoundsInstall() {}
#else

#include <SD.h>

#include "config.h"
#include "log.h"
#include "storage.h"

// Lo crea board_build.embed_files de platformio.ini a partir de la ruta del fichero
extern const uint8_t notificationStart[] asm("_binary_assets_sonidos_notificacion_mp3_start");
extern const uint8_t notificationEnd[] asm("_binary_assets_sonidos_notificacion_mp3_end");

namespace {
constexpr const char* TAG = "SONIDO";

bool hasNotificationSound() {
  StorageLock lock;
  return SD.exists(PATH_NOTIFICATION_SOUND);
}
}  // namespace

void defaultSoundsInstall() {
  if (!storageIsReady() || hasNotificationSound()) {
    return;
  }
  const size_t size = static_cast<size_t>(notificationEnd - notificationStart);
  if (!storageWriteBytes(PATH_NOTIFICATION_SOUND, notificationStart, size)) {
    logMsg(TAG, "no se pudo copiar el sonido de notificacion");
    return;
  }
  logMsg(TAG, "sonido de notificacion por defecto copiado a %s", PATH_NOTIFICATION_SOUND);
}

#endif
