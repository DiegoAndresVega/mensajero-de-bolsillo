#pragma once

// Logica pura de la seleccion multiple (Ajustes > Borrado multiple y
// Papelera > Seleccionar), sin dependencias de Arduino: se compila tambien
// en el entorno native para los tests (pio test -e native). La pantalla y lo
// que se hace al borrar estan en multi_select y delete_flow.

#include <cstddef>
#include <cstdint>

#include "button_rules.h"
#include "message_rules.h"

enum class MultiSelectAction : uint8_t {
  None,
  Toggle,     // marcar o desmarcar la fila seleccionada
  AskCancel,  // dialogo "Cancelar seleccion?" (hay alguna marcada)
  Exit,       // salir sin dialogo (no hay ninguna marcada)
  Open,       // abrir la fila seleccionada en el visor
  AskDelete   // dialogo "Borrar N seleccionados?"
};

struct MultiSelectStep {
  MessageListModel model;
  MultiSelectAction action;
};

// Pulsaciones cortas: 1/2 mueven (sin dar la vuelta, como la lista), 3 marca,
// 4 sale (con dialogo si hay marcadas). La pulsacion larga no hace nada aqui:
// mantener 3 o 4 es multiSelectOnHold.
inline MultiSelectStep multiSelectOnButton(const MessageListModel& m, const ButtonEvent& ev, uint16_t total,
                                           uint16_t visible, uint16_t checkedCount) {
  if (ev.longPress) {
    return {m, MultiSelectAction::None};
  }
  switch (ev.button) {
    case Button::BTN1_UP:
    case Button::BTN2_DOWN:
      return {messageListOnButton(m, ev, total, visible).model, MultiSelectAction::None};
    case Button::BTN3_OK:
      return {m, total > 0 ? MultiSelectAction::Toggle : MultiSelectAction::None};
    case Button::BTN4_BACK:
      return {m, checkedCount > 0 ? MultiSelectAction::AskCancel : MultiSelectAction::Exit};
    case Button::NONE:
      break;
  }
  return {m, MultiSelectAction::None};
}

// Mantener un boton UI_LONG_HOLD_MS: 3 abre el mensaje para revisarlo, 4 pide
// borrar las marcadas (sin marcadas no hace nada)
inline MultiSelectAction multiSelectOnHold(Button button, uint16_t total, uint16_t checkedCount) {
  if (button == Button::BTN3_OK) {
    return total > 0 ? MultiSelectAction::Open : MultiSelectAction::None;
  }
  if (button == Button::BTN4_BACK) {
    return checkedCount > 0 ? MultiSelectAction::AskDelete : MultiSelectAction::None;
  }
  return MultiSelectAction::None;
}

// Cambia la casilla `index` (si existe) en el array de casillas
inline void toggleChecked(bool* checked, size_t count, uint16_t index) {
  if (index < count) {
    checked[index] = !checked[index];
  }
}

inline uint16_t countSelected(const bool* selected, size_t count) {
  uint16_t n = 0;
  for (size_t i = 0; i < count; ++i) {
    if (selected[i]) {
      ++n;
    }
  }
  return n;
}
