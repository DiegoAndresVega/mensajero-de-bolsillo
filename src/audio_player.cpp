#include "audio_player.h"

#include <algorithm>
#include <atomic>

#include "config.h"
#include "log.h"
#include "storage.h"
#include "tasks.h"

#ifndef WOKWI_BUILD
#include <Audio.h>
#include <SD.h>
#endif

namespace {
constexpr const char* TAG = "AUDIO";
constexpr const char* MP3_EXT = ".mp3";
constexpr const char* WAV_EXT = ".wav";
// Fuerza a aplicar el volumen en la primera vuelta de la tarea
constexpr uint8_t VOLUME_NOT_APPLIED = UINT8_MAX;

QueueHandle_t cmdQueue = nullptr;
// Los escribe cualquier tarea y los lee la de audio, o al reves
std::atomic<uint8_t> volume{AUDIO_VOLUME_DEFAULT};
std::atomic<bool> isPlayingNow{false};
std::atomic<bool> isNotificationSoundOn{true};
// Ya se aviso de que sin config.json no se guardan los ajustes
std::atomic<bool> hasWarnedNoConfig{false};
// Lo que ve el visor (audioStatus). Solo lo escribe la tarea de audio.
std::atomic<PlayerState> shownState{PlayerState::Idle};
std::atomic<uint32_t> shownPositionSec{0};
std::atomic<uint32_t> shownDurationSec{0};
std::atomic<bool> shownInterrupted{false};

// Estado de la tarea de audio; solo lo toca ella
struct Playback {
  AudioSource source = AudioSource::None;
  uint32_t startedMs = 0;
  uint32_t idleSinceMs = 0;
  uint8_t appliedVolume = VOLUME_NOT_APPLIED;
  bool isAmpOn = false;
  // Mensaje que suena o esta en pausa. Pausar es parar y recordar el segundo:
  // asi la notificacion puede usar el decodificador y la SD queda libre.
  char messagePath[AUDIO_PATH_MAX] = "";
  bool hasPaused = false;
  uint32_t pausedSec = 0;
  bool isInterrupted = false;
  // Segundo desde el que empezo a sonar el tramo actual del mensaje
  uint32_t segmentStartSec = 0;
};

// ----- Backend: placa real --------------------------------------------------
#ifndef WOKWI_BUILD
// La libreria suena tal cual: solo pines, volumen, connecttoFS y loop. Un
// procesado propio (ganancia, limitador y graves) sonaba peor en la placa.
Audio audio;

void onAudioInfo(Audio::msg_t m) {
  // Solo lo util para depurar; ID3, caratulas, letras, etc. se ignoran
  if (m.e == Audio::evt_info || m.e == Audio::evt_eof || m.e == Audio::evt_log) {
    logMsg(TAG, "%s: %s", m.s != nullptr ? m.s : "?", m.msg != nullptr ? m.msg : "");
  }
}

bool backendInit() {
  Audio::audio_info_callback = onAudioInfo;
  // Tambien arranca la tarea interna de la libreria que decodifica y llena el
  // DMA del I2S. Va en el core 0, el contrario al que llama a loop(), como
  // recomienda la libreria.
  // SD_MODE# va directo a un GPIO a 3,3 V: el MAX98357A solo saca el canal
  // izquierdo de un MP3 estereo
  return audio.setPinout(PIN_I2S_BCLK, PIN_I2S_LRC, PIN_I2S_DIN);
}

// Devuelve true si ha empezado a sonar. startSec > 0 salta a ese segundo
// (la libreria solo sabe hacerlo con MP3 de bitrate constante).
bool backendStart(const char* path, AudioSource, uint32_t startSec) {
  StorageLock lock;
  if (!storageIsReady()) {
    return false;
  }
  // Un mensaje puede estar en WAV (copiado a mano a la SD): sin el .mp3 se busca
  // el .wav. La libreria elige el decodificador por la extension.
  String file(path);
  if (!SD.exists(file) && file.endsWith(MP3_EXT)) {
    file = file.substring(0, file.length() - strlen(MP3_EXT)) + WAV_EXT;
  }
  if (!SD.exists(file)) {
    logMsg(TAG, "%s no existe, no suena", path);
    return false;
  }
  return audio.connecttoFS(SD, file.c_str(), startSec > 0 ? static_cast<int32_t>(startSec) : -1);
}

uint32_t backendPositionSec() {
  return audio.getAudioCurrentTime();
}

uint32_t backendDurationSec() {
  return audio.getAudioFileDuration();
}

void backendStop() {
  // stopSong() cierra el fichero de la SD
  StorageLock lock;
  audio.stopSong();
}

// Avanza la reproduccion. Devuelve false cuando ha terminado.
bool backendLoop() {
  {
    // loop() lee de la SD el siguiente trozo del MP3 (hasta 64 KB)
    StorageLock lock;
    audio.loop();
  }
  return audio.isRunning();
}

void backendSetVolume(uint8_t vol) {
  audio.setVolume(vol);
}

// ----- Backend: Wokwi (simulado) --------------------------------------------
#else
uint32_t simStartMs = 0;
uint32_t simOffsetMs = 0;
char simPath[AUDIO_PATH_MAX] = "";

bool backendInit() {
  return true;
}

bool backendStart(const char* path, AudioSource source, uint32_t startSec) {
  if (source == AudioSource::Notification) {
    logMsg(TAG, "notificacion (simulado)");
    // No queda nada sonando: un PLAY posterior no tiene que cortarla
    return false;
  }
  logMsg(TAG, "reproducir: %s desde %lu s (simulado)", path, static_cast<unsigned long>(startSec));
  strlcpy(simPath, path, sizeof(simPath));
  simStartMs = millis();
  simOffsetMs = startSec * 1000U;
  return true;
}

uint32_t simPlayedMs() {
  return playedMs(simOffsetMs, simStartMs, millis(), true);
}

uint32_t backendPositionSec() {
  return simPlayedMs() / 1000U;
}

uint32_t backendDurationSec() {
  return AUDIO_SIM_DURATION_MS / 1000U;
}

void backendStop() {
  logMsg(TAG, "detenido: %s (simulado)", simPath);
}

bool backendLoop() {
  if (simPlayedMs() < AUDIO_SIM_DURATION_MS) {
    return true;
  }
  logMsg(TAG, "terminado: %s (simulado)", simPath);
  return false;
}

void backendSetVolume(uint8_t) {}
#endif

// ----- Amplificador -----------------------------------------------------------

// SD_MODE# en LOW = shutdown (unos 2 uA frente a unos 2,4 mA activo). Se
// apaga tras AUDIO_AMP_IDLE_OFF_MS sin sonar y se enciende antes de cada audio.
void setAmp(Playback& pb, bool isOn) {
  if (pb.isAmpOn == isOn) {
    return;
  }
  digitalWrite(PIN_I2S_SD_MODE, isOn ? HIGH : LOW);
  pb.isAmpOn = isOn;
  if (isOn) {
    vTaskDelay(pdMS_TO_TICKS(AUDIO_AMP_WAKE_MS));
  }
}

// ----- Tarea de audio ---------------------------------------------------------

// Publica lo que ve el visor. Una vez por vuelta de la tarea.
void publishStatus(const Playback& pb) {
  const bool isMessage = pb.source == AudioSource::Message;
  if (isMessage) {
    const uint32_t duration = backendDurationSec();
    if (duration > 0) {
      shownDurationSec = duration;
    }
  }
  shownState = playerStateOf(pb.source, pb.hasPaused);
  // Al reanudar, la libreria da 0 hasta que lee la cabecera del MP3: mientras
  // tanto se sigue mostrando el segundo de la pausa
  const uint32_t position = std::max(backendPositionSec(), pb.segmentStartSec);
  shownPositionSec = isMessage ? position : (pb.hasPaused ? pb.pausedSec : 0);
  shownInterrupted = pb.isInterrupted;
}

void markIdle(Playback& pb) {
  pb.source = AudioSource::None;
  pb.idleSinceMs = millis();
  isPlayingNow = false;
}

// Corta lo que suene (si suena algo); no toca la pausa
void stopSound(Playback& pb) {
  if (pb.source != AudioSource::None) {
    backendStop();
    markIdle(pb);
  }
}

void forgetPause(Playback& pb) {
  pb.hasPaused = false;
  pb.isInterrupted = false;
}

// Empieza a sonar `path` desde startSec; si no puede, queda en silencio
void startSound(Playback& pb, AudioSource source, const char* path, uint32_t startSec) {
  stopSound(pb);
  setAmp(pb, true);
  if (!backendStart(path, source, startSec)) {
    markIdle(pb);
    return;
  }
  pb.source = source;
  pb.startedMs = millis();
  pb.segmentStartSec = startSec;
  isPlayingNow = true;
}

// Para el mensaje y recuerda el segundo para seguir luego
void pauseMessage(Playback& pb, bool isInterrupted) {
  pb.pausedSec = backendPositionSec();
  stopSound(pb);
  pb.hasPaused = true;
  pb.isInterrupted = isInterrupted;
  logMsg(TAG, "%s en %lu s", isInterrupted ? "interrumpido por la notificacion" : "pausa",
         static_cast<unsigned long>(pb.pausedSec));
}

void resumeMessage(Playback& pb) {
  const uint32_t fromSec = pb.pausedSec;
  forgetPause(pb);
  logMsg(TAG, "sigue desde %lu s", static_cast<unsigned long>(fromSec));
  startSound(pb, AudioSource::Message, pb.messagePath, fromSec);
}

void playMessage(Playback& pb, const char* path) {
  forgetPause(pb);
  strlcpy(pb.messagePath, path, sizeof(pb.messagePath));
  shownDurationSec = 0;
  startSound(pb, AudioSource::Message, path, 0);
}

void handleCommand(const AudioCmd& cmd, Playback& pb) {
  switch (decideAudioAction(cmd.type, pb.source, pb.hasPaused)) {
    case AudioAction::Start:
      if (cmd.type == AUDIO_CMD_NOTIFY) {
        startSound(pb, AudioSource::Notification, PATH_NOTIFICATION_SOUND, 0);
      } else {
        playMessage(pb, cmd.path);
      }
      return;
    case AudioAction::Stop:
      stopSound(pb);
      forgetPause(pb);
      return;
    case AudioAction::Pause:
      pauseMessage(pb, false);
      return;
    case AudioAction::Interrupt:
      pauseMessage(pb, true);
      startSound(pb, AudioSource::Notification, PATH_NOTIFICATION_SOUND, 0);
      return;
    case AudioAction::Resume:
      resumeMessage(pb);
      return;
    case AudioAction::StopSound:
      stopSound(pb);
      return;
    case AudioAction::Ignore:
      if (cmd.type == AUDIO_CMD_NOTIFY) {
        logMsg(TAG, "ya suena otra notificacion: se descarta");
      }
      return;
  }
}

void advancePlayback(Playback& pb) {
  if (hasElapsed(pb.startedMs, millis(), AUDIO_MAX_PLAY_MS)) {
    logMsg(TAG, "mas de %lu s sonando: se detiene", static_cast<unsigned long>(AUDIO_MAX_PLAY_MS / 1000));
    backendStop();
    markIdle(pb);
    return;
  }
  if (!backendLoop()) {
    markIdle(pb);
  }
}

void applyVolume(Playback& pb) {
  const uint8_t wanted = volume.load();
  if (wanted != pb.appliedVolume) {
    backendSetVolume(wanted);
    pb.appliedVolume = wanted;
  }
}

void audioTask(void*) {
  Playback pb;
  pb.isAmpOn = true;  // audioInit lo deja encendido
  pb.idleSinceMs = millis();
  AudioCmd cmd;
  for (;;) {
    const bool isBusy = pb.source != AudioSource::None;
    // En reposo la espera en la cola hace de pausa; sonando no se espera
    const TickType_t waitTicks = isBusy ? 0 : pdMS_TO_TICKS(AUDIO_IDLE_POLL_MS);
    if (xQueueReceive(cmdQueue, &cmd, waitTicks) == pdTRUE) {
      handleCommand(cmd, pb);
      publishStatus(pb);
    }
    applyVolume(pb);
    if (pb.source != AudioSource::None) {
      advancePlayback(pb);
      publishStatus(pb);
      vTaskDelay(pdMS_TO_TICKS(AUDIO_LOOP_INTERVAL_MS));
    } else if (pb.isAmpOn && hasElapsed(pb.idleSinceMs, millis(), AUDIO_AMP_IDLE_OFF_MS)) {
      setAmp(pb, false);
    }
  }
}

// Sin config.json los cambios valen hasta reiniciar: se avisa una sola vez
// en vez de intentar guardar (y fallar) en cada pulsacion
bool canSaveSettings() {
  if (storageHasConfigFile()) {
    return true;
  }
  if (!hasWarnedNoConfig.exchange(true)) {
    logMsg(TAG, "sin config.json, ajustes no persistentes");
  }
  return false;
}

bool enqueue(AudioCmdType type, const char* path) {
  if (cmdQueue == nullptr) {
    return false;
  }
  AudioCmd cmd{};
  cmd.type = type;
  strlcpy(cmd.path, path != nullptr ? path : "", sizeof(cmd.path));
  if (xQueueSend(cmdQueue, &cmd, 0) != pdTRUE) {
    logMsg(TAG, "cola llena, se descarta el comando %d", static_cast<int>(type));
    return false;
  }
  return true;
}
}  // namespace

bool audioInit(uint8_t initialVolume, bool isNotificationSoundOnAtStart) {
  if (cmdQueue != nullptr) {
    return true;
  }
  volume = clampAudioVolume(initialVolume);
  isNotificationSoundOn = isNotificationSoundOnAtStart;
  pinMode(PIN_I2S_SD_MODE, OUTPUT);
  digitalWrite(PIN_I2S_SD_MODE, HIGH);

  if (!backendInit()) {
    logMsg(TAG, "fallo al iniciar el I2S");
    digitalWrite(PIN_I2S_SD_MODE, LOW);
    return false;
  }
  cmdQueue = xQueueCreate(AUDIO_QUEUE_LEN, sizeof(AudioCmd));
  if (cmdQueue == nullptr) {
    logMsg(TAG, "no se pudo crear la cola");
    return false;
  }
  if (xTaskCreatePinnedToCore(audioTask, "audio", STACK_AUDIO_TASK, nullptr, PRIO_AUDIO_TASK, nullptr, CORE_APP) !=
      pdPASS) {
    logMsg(TAG, "no se pudo crear la tarea");
    vQueueDelete(cmdQueue);
    cmdQueue = nullptr;
    return false;
  }
  logMsg(TAG, "listo, volumen %u", static_cast<unsigned>(volume.load()));
  return true;
}

void audioSetVolume(uint8_t vol) {
  const uint8_t clamped = clampAudioVolume(vol);
  // Sin cambios no se escribe en la SD
  if (volume.exchange(clamped) == clamped) {
    return;
  }
  logMsg(TAG, "volumen %u", static_cast<unsigned>(clamped));
  if (canSaveSettings()) {
    tasksRequestSaveVolume(clamped);
  }
}

uint8_t audioGetVolume() {
  return volume.load();
}

void audioVolumeUp() {
  audioSetVolume(clampAudioVolume(static_cast<int32_t>(volume.load()) + 1));
}

void audioVolumeDown() {
  audioSetVolume(clampAudioVolume(static_cast<int32_t>(volume.load()) - 1));
}

bool audioPlay(const char* path) {
  if (!isValidAudioPath(path)) {
    logMsg(TAG, "ruta de audio no valida, se ignora");
    return false;
  }
  return enqueue(AUDIO_CMD_PLAY, path);
}

void audioStop() {
  enqueue(AUDIO_CMD_STOP, nullptr);
}

bool audioIsPlaying() {
  return isPlayingNow.load();
}

void audioNotifyNewMessage() {
  if (!isNotificationSoundOn.load()) {
    logMsg(TAG, "sonido de notificaciones en OFF: no suena");
    return;
  }
  enqueue(AUDIO_CMD_NOTIFY, nullptr);
}

void audioSetNotificationSound(bool isOn) {
  if (isNotificationSoundOn.exchange(isOn) == isOn) {
    return;
  }
  logMsg(TAG, "sonido de notificaciones %s", isOn ? "ON" : "OFF");
  if (canSaveSettings()) {
    tasksRequestSaveNotificationSound(isOn);
  }
}

bool audioNotificationSoundOn() {
  return isNotificationSoundOn.load();
}

void audioPause() {
  enqueue(AUDIO_CMD_PAUSE, nullptr);
}

void audioResume() {
  enqueue(AUDIO_CMD_RESUME, nullptr);
}

void audioStopNotification() {
  enqueue(AUDIO_CMD_STOP_NOTIFY, nullptr);
}

AudioStatus audioStatus() {
  return {shownState.load(), shownPositionSec.load(), shownDurationSec.load(), shownInterrupted.load()};
}

