#pragma once

#include <cstdint>

#include "display.h"
#include "settings_rules.h"

// Dibujo del menu Ajustes y sus subpantallas: Volumen, Sonido de
// notificaciones y WiFi. La
// navegacion es logica pura (settingsOnButton en settings_rules.h); aplicar
// los cambios es cosa de ui_manager. Solo lo usa la tarea de UI.

// Todo lo que se pinta. Si no cambia, no hay que repintar.
struct SettingsView {
  SettingsModel model;
  uint8_t volume = 0;
  bool isSoundOn = true;
  bool isOnline = false;
  WifiScreenStatus wifi = WifiScreenStatus::Disconnected;
  // Numero grande en Volumen tras un cambio
  bool isVolumeBig = false;
  // En WiFi sube cada segundo para refrescar IP y RSSI; en el resto, 0
  uint32_t refreshTick = 0;
};

bool operator==(const SettingsView& a, const SettingsView& b);

// Pantalla completa: la lista o la subpantalla de view.model
void settingsDraw(LGFX_Sprite& canvas, const SettingsView& view);
