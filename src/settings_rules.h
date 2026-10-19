#pragma once

// Logica pura del menu Ajustes, sin dependencias de Arduino: se compila
// tambien en el entorno native para los tests (pio test -e native). Dibujar
// es cosa de settings; guardar, de audio_player y storage. El mensaje
// personal ya no esta aqui: es la pantalla Info del menu principal
// (info_rules.h).

#include <cstddef>
#include <cstdint>

#include "audio_rules.h"
#include "button_rules.h"

// ----- Navegacion -----------------------------------------------------------------

enum class SettingsItem : uint8_t { VOLUME, NOTIFICATION_SOUND, WIFI, MULTI_DELETE };
constexpr uint8_t SETTINGS_ITEM_COUNT = 4;

// La lista y una subpantalla por cada fila. "Borrado multiple" no esta: es
// otro estado de la UI (accion OpenMultiDelete). La Papelera esta en el menu
// principal.
enum class SettingsScreen : uint8_t { LIST, VOLUME, NOTIFICATION_SOUND, WIFI };

struct SettingsModel {
  uint8_t selected = 0;
  SettingsScreen screen = SettingsScreen::LIST;
};

inline bool operator==(const SettingsModel& a, const SettingsModel& b) {
  return a.selected == b.selected && a.screen == b.screen;
}

enum class SettingsAction : uint8_t {
  None,
  Exit,  // al menu principal
  VolumeUp,
  VolumeDown,
  ToggleSound,
  ReconnectWifi,
  OpenMultiDelete
};

struct SettingsStep {
  SettingsModel model;
  SettingsAction action;
};

inline SettingsScreen settingsScreenOf(uint8_t item) {
  switch (static_cast<SettingsItem>(item)) {
    case SettingsItem::VOLUME:
      return SettingsScreen::VOLUME;
    case SettingsItem::NOTIFICATION_SOUND:
      return SettingsScreen::NOTIFICATION_SOUND;
    case SettingsItem::WIFI:
      return SettingsScreen::WIFI;
    case SettingsItem::MULTI_DELETE:
      break;
  }
  return SettingsScreen::LIST;
}

// Lista: 1/2 mueven dando la vuelta (como el menu principal), 3 entra, 4 sale.
inline SettingsStep settingsListOnButton(const SettingsModel& m, const ButtonEvent& ev) {
  switch (ev.button) {
    case Button::BTN1_UP:
      return {{static_cast<uint8_t>((m.selected + SETTINGS_ITEM_COUNT - 1) % SETTINGS_ITEM_COUNT), m.screen},
              SettingsAction::None};
    case Button::BTN2_DOWN:
      return {{static_cast<uint8_t>((m.selected + 1) % SETTINGS_ITEM_COUNT), m.screen}, SettingsAction::None};
    case Button::BTN3_OK:
      switch (static_cast<SettingsItem>(m.selected)) {
        case SettingsItem::MULTI_DELETE:
          return {m, SettingsAction::OpenMultiDelete};
        default:
          return {{m.selected, settingsScreenOf(m.selected)}, SettingsAction::None};
      }
    case Button::BTN4_BACK:
      return {m, SettingsAction::Exit};
    case Button::NONE:
      break;
  }
  return {m, SettingsAction::None};
}

// Subpantallas: 4 vuelve a la lista con la misma fila seleccionada.
// Volumen: 1 sube, 2 baja. Sonido: 3 alterna. WiFi: 3 reconecta. La
// pulsacion larga no hace nada; el doble toque cuenta como pulsacion normal.
inline SettingsStep settingsOnButton(const SettingsModel& m, const ButtonEvent& ev) {
  if (ev.longPress) {
    return {m, SettingsAction::None};
  }
  if (m.screen == SettingsScreen::LIST) {
    return settingsListOnButton(m, ev);
  }
  if (ev.button == Button::BTN4_BACK) {
    return {{m.selected, SettingsScreen::LIST}, SettingsAction::None};
  }
  switch (m.screen) {
    case SettingsScreen::VOLUME:
      if (ev.button == Button::BTN1_UP) {
        return {m, SettingsAction::VolumeUp};
      }
      return {m, ev.button == Button::BTN2_DOWN ? SettingsAction::VolumeDown : SettingsAction::None};
    case SettingsScreen::NOTIFICATION_SOUND:
      return {m, ev.button == Button::BTN3_OK ? SettingsAction::ToggleSound : SettingsAction::None};
    case SettingsScreen::WIFI:
      return {m, ev.button == Button::BTN3_OK ? SettingsAction::ReconnectWifi : SettingsAction::None};
    case SettingsScreen::LIST:
      break;
  }
  return {m, SettingsAction::None};
}

// ----- Valores ------------------------------------------------------------------

// Volumen tras una pulsacion de `step`, dentro de 0-AUDIO_VOLUME_MAX
inline uint8_t settingsVolumeAfter(uint8_t volume, bool isUp, uint8_t step) {
  const int32_t delta = isUp ? step : -static_cast<int32_t>(step);
  return clampAudioVolume(static_cast<int32_t>(volume) + delta);
}

inline bool settingsToggledSound(bool isOn) {
  return !isOn;
}

inline const char* settingsOnOffLabel(bool isOn) {
  return isOn ? "ON" : "OFF";
}

// ----- WiFi -----------------------------------------------------------------------

enum class WifiScreenStatus : uint8_t { Connected, Connecting, Disconnected };

// Tras pulsar "reconectar" se muestra "Conectando..." al menos minShowMs (para
// que se vea el intento aunque ya hubiera conexion) y como mucho timeoutMs.
inline WifiScreenStatus wifiScreenStatus(bool isConnected, bool isReconnecting, uint32_t msSinceRequest,
                                         uint32_t minShowMs, uint32_t timeoutMs) {
  const bool isWaiting = isReconnecting && msSinceRequest < timeoutMs;
  if (isWaiting && (!isConnected || msSinceRequest < minShowMs)) {
    return WifiScreenStatus::Connecting;
  }
  return isConnected ? WifiScreenStatus::Connected : WifiScreenStatus::Disconnected;
}
