#pragma once

#include <cstdint>

#include "audio_rules.h"

// Reproductor de audio (MAX98357A por I2S). Las funciones se pueden llamar
// desde cualquier tarea y no bloquean: dejan un comando en la cola y la tarea
// de audio (core 1) hace el trabajo. Solo suena un audio a la vez; las reglas
// de que corta a que estan en audio_rules.h.
//
// En Wokwi (WOKWI_BUILD) no se toca el I2S: la reproduccion se simula por serie.

// Amplificador, I2S y tarea de audio. Una vez en setup(), despues de
// storageInit() y antes de tasksStart(). Devuelve false si falla el I2S.
bool audioInit(uint8_t initialVolume, bool isNotificationSoundOn);

// 0-21. Si el valor cambia, se guarda en config.json via la tarea de storage.
// Sin config.json no se guarda (se avisa una vez por arranque).
void audioSetVolume(uint8_t vol);
uint8_t audioGetVolume();
void audioVolumeUp();
void audioVolumeDown();

// MP3 de la SD. Devuelve false si la ruta no es valida o la cola esta llena;
// si el fichero no existe se ve en el log de la tarea de audio.
bool audioPlay(const char* path);
// Silencio: corta lo que suene y olvida el mensaje en pausa
void audioStop();
// true si suena algo (mensaje o notificacion): la SD se esta leyendo
bool audioIsPlaying();

// Suena /sonidos/notificacion.mp3. Si suena un mensaje lo pausa (queda
// interrumpido hasta audioResume); si suena otra notificacion no hace nada.
// Si el fichero no existe o el sonido de notificaciones esta en OFF, no
// suena nada (y no pausa nada).
void audioNotifyNewMessage();

// Sonido de notificaciones (Ajustes). Solo afecta a audioNotifyNewMessage:
// los mensajes de voz suenan igual. Si cambia se guarda como el volumen.
void audioSetNotificationSound(bool isOn);
bool audioNotificationSoundOn();

// Pausa del mensaje: se para y se recuerda el segundo. No se lee la SD.
void audioPause();
// Sigue el mensaje en pausa desde ese segundo (corta la notificacion si suena)
void audioResume();
// Corta solo el sonido de notificacion
void audioStopNotification();

// Lo que ve el visor de audio. Se puede leer desde cualquier tarea.
struct AudioStatus {
  PlayerState state;
  uint32_t positionSec;
  // 0 si aun no se sabe. Se mantiene al terminar, hasta el siguiente audioPlay
  uint32_t durationSec;
  // El mensaje esta en pausa porque sono una notificacion encima
  bool isInterrupted;
};
AudioStatus audioStatus();
