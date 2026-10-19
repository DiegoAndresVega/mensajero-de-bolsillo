#pragma once

#include "config.h"

// Bucle de la UI: maquina de estados (ui_rules.h), botones, ojos, menu y
// apagado por inactividad. Una tarea en el core 1 a UI_FPS fotogramas por
// segundo dibuja en un framebuffer en PSRAM y solo envia a la pantalla lo que
// cambia. Mientras la SD o el audio estan ocupados no se anima: se muestra
// una vez un reloj de arena en la esquina y se sigue al liberarse el bus.
// Excepciones: el visor de audio (su onda, a AUDIO_VIEW_FPS) y el aviso de
// mensaje nuevo, que va encima de cualquier pantalla.

// Pantalla, framebuffer, botones y tarea de UI. En setup(), despues de
// storageInit() y antes de tasksStart(): al arrancar, LovyanGFX reinicia el
// bus SPI y no puede haber otra tarea usando la SD. Devuelve false si no hay
// pantalla o memoria para el framebuffer. De `config` solo se usa el mensaje
// personal de Info (se copia).
bool uiInit(const AppConfig& config);
