#pragma once

// Reglas del reproductor sin dependencias de Arduino: se compilan tambien en
// el entorno native para los tests (pio test -e native).

#include <cstddef>
#include <cstdint>
#include <cstring>

#include "utils.h"

// Escala de la libreria ESP32-audioI2S: 0 = silencio, 21 = maximo
constexpr uint8_t AUDIO_VOLUME_MAX = 21;
constexpr uint8_t AUDIO_VOLUME_DEFAULT = 15;
// Tamano de AudioCmd::path, terminador incluido
constexpr size_t AUDIO_PATH_MAX = 64;

typedef enum {
  AUDIO_CMD_PLAY,
  AUDIO_CMD_STOP,
  AUDIO_CMD_NOTIFY,
  AUDIO_CMD_PAUSE,
  AUDIO_CMD_RESUME,
  // Corta solo el sonido de notificacion (cancelar el aviso en pantalla)
  AUDIO_CMD_STOP_NOTIFY
} AudioCmdType;

struct AudioCmd {
  AudioCmdType type;
  char path[AUDIO_PATH_MAX];
};

// Que esta sonando ahora mismo
enum class AudioSource : uint8_t { None, Message, Notification };

// Estado del audio de un mensaje, que es lo que ve el visor. En pausa no
// suena nada y no se lee la SD: se recuerda la ruta y el segundo.
enum class PlayerState : uint8_t { Idle, Playing, Paused };

enum class AudioAction : uint8_t {
  Start,      // empieza a sonar (corta lo que suene y olvida la pausa)
  Stop,       // silencio y olvida la pausa
  Pause,      // para el mensaje y recuerda por donde iba
  Resume,     // sigue el mensaje en pausa (corta la notificacion si suena)
  Interrupt,  // pausa el mensaje para que suene la notificacion
  StopSound,  // corta lo que suene sin tocar la pausa
  Ignore
};

// Un solo audio a la vez. PLAY corta lo que suene. NOTIFY no corta otra
// notificacion, y a un mensaje lo pausa (luego la UI lo reanuda). hasPaused:
// hay un mensaje en pausa esperando.
inline AudioAction decideAudioAction(AudioCmdType cmd, AudioSource playing, bool hasPaused = false) {
  switch (cmd) {
    case AUDIO_CMD_PLAY:
      return AudioAction::Start;
    case AUDIO_CMD_STOP:
      return playing == AudioSource::None && !hasPaused ? AudioAction::Ignore : AudioAction::Stop;
    case AUDIO_CMD_NOTIFY:
      if (playing == AudioSource::Message) {
        return AudioAction::Interrupt;
      }
      return playing == AudioSource::None ? AudioAction::Start : AudioAction::Ignore;
    case AUDIO_CMD_PAUSE:
      return playing == AudioSource::Message ? AudioAction::Pause : AudioAction::Ignore;
    case AUDIO_CMD_RESUME:
      return hasPaused ? AudioAction::Resume : AudioAction::Ignore;
    case AUDIO_CMD_STOP_NOTIFY:
      return playing == AudioSource::Notification ? AudioAction::StopSound : AudioAction::Ignore;
  }
  return AudioAction::Ignore;
}

// Estado que ve el visor segun lo que suena y si hay un mensaje en pausa
inline PlayerState playerStateOf(AudioSource playing, bool hasPaused) {
  if (playing == AudioSource::Message) {
    return PlayerState::Playing;
  }
  return hasPaused ? PlayerState::Paused : PlayerState::Idle;
}

// Milisegundos reproducidos de un audio que ha sonado a tramos: los tramos
// ya cerrados (accumulatedMs) mas el actual si esta sonando. Resta sin signo:
// aguanta la vuelta de millis().
inline uint32_t playedMs(uint32_t accumulatedMs, uint32_t segmentStartMs, uint32_t nowMs, bool isRunning) {
  return accumulatedMs + (isRunning ? nowMs - segmentStartMs : 0);
}

inline uint8_t clampAudioVolume(int32_t volume) {
  if (volume < 0) {
    return 0;
  }
  return volume > AUDIO_VOLUME_MAX ? AUDIO_VOLUME_MAX : static_cast<uint8_t>(volume);
}

// Ruta absoluta en la SD que cabe en AudioCmd::path y no sube de carpeta
inline bool isValidAudioPath(const char* path) {
  if (path == nullptr || path[0] != '/') {
    return false;
  }
  const size_t len = strnlen(path, AUDIO_PATH_MAX);
  return len < AUDIO_PATH_MAX && strstr(path, "..") == nullptr;
}
