#pragma once

// Reglas de la UI sin dependencias de Arduino: se compilan tambien en el
// entorno native para los tests (pio test -e native). Aqui solo se decide
// a que pantalla se pasa; dibujar es cosa de ui_manager, eyes y menu. Las
// caras de los ojos estan en eye_rules.h y eye_shapes.h.

#include <cstdint>

#include "button_rules.h"

// ----- Maquina de estados de la pantalla ------------------------------------

enum class UiState : uint8_t {
  BOOT,          // arranque: ojos felices unos segundos
  EYES,          // ojos en reposo
  MENU,          // menu principal
  MESSAGE_LIST,  // lista de mensajes de la SD (todos o solo favoritos)
  MESSAGE_VIEW,  // visor de un mensaje de texto
  AUDIO_VIEW,    // visor de un mensaje de voz
  SETTINGS,      // menu Ajustes y sus subpantallas (settings_rules.h)
  INFO,          // mensaje personal del propietario (info_rules.h)
  PHOTOS,        // fotos de /fotos en la SD (photo_rules.h)
  MULTI_DELETE,  // Ajustes > Borrado multiple (multi_select_rules.h)
  TRASH,         // Papelera del menu principal (trash_menu_rules.h)
  OFF            // retroiluminacion a 0
};

struct UiTimeouts {
  uint32_t bootMs;
  uint32_t idleToEyesMs;
  uint32_t eyesToOffMs;
  // En el visor se lee: necesita mas margen que el menu
  uint32_t viewIdleToEyesMs;
};

// Que hacer con un evento de boton segun la pantalla
enum class UiButtonRoute : uint8_t {
  Ignore,     // EYES/OFF con un toque simple o largo: no despierta
  Wake,       // doble toque en EYES/OFF: al menu
  Screen,     // lo gestiona la pantalla activa (menu, lista...)
  BootInput   // durante el arranque: solo cuenta como actividad
};

inline UiButtonRoute uiRouteButton(UiState state, const ButtonEvent& ev) {
  switch (state) {
    case UiState::BOOT:
      return UiButtonRoute::BootInput;
    case UiState::EYES:
    case UiState::OFF:
      return ev.doubleTap ? UiButtonRoute::Wake : UiButtonRoute::Ignore;
    case UiState::MENU:
    case UiState::MESSAGE_LIST:
    case UiState::MESSAGE_VIEW:
    case UiState::AUDIO_VIEW:
    case UiState::SETTINGS:
    case UiState::INFO:
    case UiState::PHOTOS:
    case UiState::MULTI_DELETE:
    case UiState::TRASH:
      return UiButtonRoute::Screen;
  }
  return UiButtonRoute::Ignore;
}

// Transiciones por tiempo. msInState cuenta desde que se entro en el estado;
// msSinceInput, desde el ultimo boton aceptado. En EYES los toques simples se
// ignoran y no retrasan el apagado, por eso ahi se usa msInState. En el visor
// de audio, mientras suena, ui_manager cuenta el audio como actividad.
inline UiState uiStateAfterTime(UiState state, uint32_t msInState, uint32_t msSinceInput, bool hadBootInput,
                                const UiTimeouts& t) {
  switch (state) {
    case UiState::BOOT:
      if (msInState < t.bootMs) {
        return state;
      }
      return hadBootInput ? UiState::MENU : UiState::EYES;
    case UiState::EYES:
      return msInState >= t.eyesToOffMs ? UiState::OFF : state;
    case UiState::MENU:
    case UiState::MESSAGE_LIST:
    case UiState::SETTINGS:
    case UiState::INFO:
    case UiState::PHOTOS:
    case UiState::MULTI_DELETE:
    case UiState::TRASH:
      return msSinceInput >= t.idleToEyesMs ? UiState::EYES : state;
    case UiState::MESSAGE_VIEW:
    case UiState::AUDIO_VIEW:
      return msSinceInput >= t.viewIdleToEyesMs ? UiState::EYES : state;
    case UiState::OFF:
      return state;
  }
  return state;
}

// ----- Mantener un boton -------------------------------------------------------

// Mantener pulsado un boton UI_LONG_HOLD_MS (mas que la pulsacion larga
// normal): la pulsacion larga empieza a contar y se mira el GPIO hasta el final.
enum class HoldState : uint8_t { Released, Waiting, Done };

inline HoldState holdStateAt(bool isPressed, uint32_t startMs, uint32_t nowMs, uint32_t holdMs) {
  if (!isPressed) {
    return HoldState::Released;
  }
  return nowMs - startMs >= holdMs ? HoldState::Done : HoldState::Waiting;
}

// ----- Menu principal ---------------------------------------------------------

enum class MenuItem : uint8_t { MESSAGES, FAVORITES, PHOTOS, TRASH, SETTINGS, INFO };
constexpr uint8_t MENU_ITEM_COUNT = 6;

// Cada fila abre otro estado: "Mensajes" y "Favoritos" MESSAGE_LIST,
// "Fotos" PHOTOS, "Papelera" TRASH, "Ajustes" SETTINGS e "Info" INFO.
struct MenuModel {
  uint8_t selected = 0;
};

inline bool operator==(const MenuModel& a, const MenuModel& b) {
  return a.selected == b.selected;
}

enum class MenuExit : uint8_t { None, ToEyes, ToMessages, ToFavorites, ToPhotos, ToTrash, ToSettings, ToInfo };

struct MenuStep {
  MenuModel model;
  MenuExit exit;
};

// Devuelve un modelo nuevo; no toca el recibido. Un doble toque cuenta como
// una pulsacion normal (su primer toque ya llego antes como toque simple).
// La pulsacion larga no hace nada todavia.
inline MenuStep menuOnButton(const MenuModel& m, const ButtonEvent& ev) {
  if (ev.longPress) {
    return {m, MenuExit::None};
  }
  switch (ev.button) {
    case Button::BTN1_UP:
      return {{static_cast<uint8_t>((m.selected + MENU_ITEM_COUNT - 1) % MENU_ITEM_COUNT)}, MenuExit::None};
    case Button::BTN2_DOWN:
      return {{static_cast<uint8_t>((m.selected + 1) % MENU_ITEM_COUNT)}, MenuExit::None};
    case Button::BTN3_OK:
      switch (static_cast<MenuItem>(m.selected)) {
        case MenuItem::MESSAGES:
          return {m, MenuExit::ToMessages};
        case MenuItem::FAVORITES:
          return {m, MenuExit::ToFavorites};
        case MenuItem::PHOTOS:
          return {m, MenuExit::ToPhotos};
        case MenuItem::TRASH:
          return {m, MenuExit::ToTrash};
        case MenuItem::SETTINGS:
          return {m, MenuExit::ToSettings};
        case MenuItem::INFO:
          return {m, MenuExit::ToInfo};
      }
      return {m, MenuExit::None};
    case Button::BTN4_BACK:
      return {m, MenuExit::ToEyes};
    case Button::NONE:
      break;
  }
  return {m, MenuExit::None};
}
