#pragma once

// Logica pura de los dialogos de confirmacion, sin dependencias de Arduino: se compila tambien en el entorno native para los
// tests (pio test -e native). Dibujar es cosa de dialog; que se hace al
// confirmar, de delete_flow.

#include <cstdint>

#include "button_rules.h"

// ----- Dialogo de confirmacion ------------------------------------------------------

// Un dialogo de `steps` preguntas seguidas ("Borrar?" y "Seguro?" son 2). Se
// confirma al decir que si a la ultima; un no en cualquiera cancela.
struct DialogModel {
  uint8_t step = 0;
  uint8_t steps = 0;
};

inline bool dialogIsOpen(const DialogModel& m) {
  return m.steps > 0;
}

inline DialogModel dialogOpen(uint8_t steps) {
  return {0, steps};
}

enum class DialogResult : uint8_t { Pending, Confirmed, Cancelled };

struct DialogStep {
  DialogModel model;
  DialogResult result;
};

// 3 = si (siguiente pregunta o confirmar), 4 = no. La pulsacion larga y 1/2
// no hacen nada: un dialogo de borrado no se acepta por accidente.
inline DialogStep dialogOnButton(const DialogModel& m, const ButtonEvent& ev) {
  if (!dialogIsOpen(m) || ev.longPress) {
    return {m, DialogResult::Pending};
  }
  if (ev.button == Button::BTN4_BACK) {
    return {DialogModel{}, DialogResult::Cancelled};
  }
  if (ev.button != Button::BTN3_OK) {
    return {m, DialogResult::Pending};
  }
  const auto next = static_cast<uint8_t>(m.step + 1);
  if (next >= m.steps) {
    return {DialogModel{}, DialogResult::Confirmed};
  }
  return {{next, m.steps}, DialogResult::Pending};
}
