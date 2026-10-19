#pragma once

// Logica pura del menu de la Papelera, sin dependencias de Arduino: se
// compila tambien en el entorno native para los tests (pio test -e native).
// Dibujarlo es cosa de trash_menu.

#include <cstdint>

#include "button_rules.h"

// "Cancelar borrado pendiente" solo sale si hay cola pendiente
enum class TrashMenuItem : uint8_t { VIEW, SELECT, EMPTY, CANCEL_PENDING };

inline uint8_t trashMenuItemCount(uint16_t pendingCount) {
  return pendingCount > 0 ? 4 : 3;
}

enum class TrashMenuAction : uint8_t {
  None,
  Exit,           // a Ajustes
  OpenList,       // Ver mensajes
  OpenSelect,     // Seleccionar
  AskEmpty,       // Vaciar papelera
  AskCancelQueue  // Cancelar borrado pendiente
};

struct TrashMenuStep {
  uint8_t selected;
  TrashMenuAction action;
};

// "Vaciar papelera" vacia lo que se ve y reintenta lo que ya estaba en la
// cola. Solo si no hay nada de nada se avisa sin preguntar; si no, siempre
// las dos confirmaciones, haya conexion o no.
enum class TrashEmptyDecision : uint8_t { NothingToEmpty, Confirm };

inline TrashEmptyDecision trashEmptyDecision(uint16_t visibleCount, uint16_t pendingCount) {
  return visibleCount == 0 && pendingCount == 0 ? TrashEmptyDecision::NothingToEmpty : TrashEmptyDecision::Confirm;
}

// La seleccion dentro del menu actual: si la cola se vacio mientras estaba
// en la ultima fila, pasa a la anterior
inline uint8_t trashMenuClamp(uint8_t selected, uint16_t pendingCount) {
  const uint8_t count = trashMenuItemCount(pendingCount);
  return selected < count ? selected : static_cast<uint8_t>(count - 1);
}

// 1/2 mueven dando la vuelta (como los otros menus), 3 entra, 4 sale a
// Ajustes. La pulsacion larga no hace nada.
inline TrashMenuStep trashMenuOnButton(uint8_t selected, const ButtonEvent& ev, uint16_t pendingCount) {
  const uint8_t count = trashMenuItemCount(pendingCount);
  const uint8_t sel = trashMenuClamp(selected, pendingCount);
  if (ev.longPress) {
    return {sel, TrashMenuAction::None};
  }
  switch (ev.button) {
    case Button::BTN1_UP:
      return {static_cast<uint8_t>((sel + count - 1) % count), TrashMenuAction::None};
    case Button::BTN2_DOWN:
      return {static_cast<uint8_t>((sel + 1) % count), TrashMenuAction::None};
    case Button::BTN3_OK:
      switch (static_cast<TrashMenuItem>(sel)) {
        case TrashMenuItem::VIEW:
          return {sel, TrashMenuAction::OpenList};
        case TrashMenuItem::SELECT:
          return {sel, TrashMenuAction::OpenSelect};
        case TrashMenuItem::EMPTY:
          return {sel, TrashMenuAction::AskEmpty};
        case TrashMenuItem::CANCEL_PENDING:
          return {sel, TrashMenuAction::AskCancelQueue};
      }
      return {sel, TrashMenuAction::None};
    case Button::BTN4_BACK:
      return {sel, TrashMenuAction::Exit};
    case Button::NONE:
      break;
  }
  return {sel, TrashMenuAction::None};
}
