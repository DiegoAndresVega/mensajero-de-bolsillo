#include <Arduino.h>

#include "audio_player.h"
#include "default_sounds.h"
#include "deletion_queue.h"
#include "config.h"
#include "log.h"
#include "state.h"
#include "storage.h"
#include "tasks.h"
#include "ui_manager.h"

namespace {
constexpr const char* TAG = "MAIN";
// Deja tiempo a que el monitor serie se conecte antes de los primeros logs
constexpr uint32_t BOOT_SERIAL_DELAY_MS = 1500;

// Vive todo el programa: las tareas guardan una referencia
AppConfig appConfig;
}  // namespace

void setup() {
  logInit();
  delay(BOOT_SERIAL_DELAY_MS);
  logMsg(TAG, "llavero %s arrancando | flash %lu MB, PSRAM %lu KB, CPU %lu MHz", FW_VERSION,
         static_cast<unsigned long>(ESP.getFlashChipSize() / (1024 * 1024)),
         static_cast<unsigned long>(ESP.getPsramSize() / 1024), static_cast<unsigned long>(getCpuFrequencyMhz()));

  stateInit();
  const bool isSdReady = storageInit();
  if (isSdReady) {
    stateLoad();
    defaultSoundsInstall();
  }
  // Tambien sin SD: la UI y la red la consultan
  deletionQueueInit();
  // En Wokwi readConfig puede dar config simulada aunque la SD no este
  const bool hasConfig = readConfig(appConfig);
  // Antes de las tareas: la de red ya puede pedir la notificacion. Sin config
  // valida se usa el volumen por defecto de AppConfig.
  if (!audioInit(appConfig.audioVolume, appConfig.notificationSound)) {
    logMsg(TAG, "audio no disponible, se sigue sin sonido");
  }
  // Antes de tasksStart: al iniciar la pantalla LovyanGFX reinicia el bus SPI
  // y ninguna tarea puede estar usando la SD
  if (!uiInit(appConfig)) {
    logMsg(TAG, "UI no disponible, se sigue sin pantalla");
  }
  tasksStart(appConfig, hasConfig);
}

void loop() {
  // Todo el trabajo esta en las tareas FreeRTOS
  vTaskDelay(portMAX_DELAY);
}
