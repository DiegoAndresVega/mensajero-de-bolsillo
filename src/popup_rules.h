#pragma once

// Logica pura del aviso de mensaje nuevo (sobre en grande), sin dependencias
// de Arduino: se compila tambien en el entorno native para los tests (pio
// test -e native). Dibujar es cosa de notification_popup.

#include <cmath>
#include <cstdint>

#include "button_rules.h"
#include "utils.h"

// Desplazamiento horizontal del sobre a los msShown ms: seno de `hz` vueltas
// por segundo y amplitud amplitudePx. Empieza en 0 (sin salto al aparecer).
inline int32_t popupWobbleX(uint32_t msShown, uint32_t hz, int32_t amplitudePx) {
  constexpr float FULL_TURN_RAD = 6.2831853F;
  constexpr float MS_PER_S = 1000.0F;
  const float phase = FULL_TURN_RAD * static_cast<float>(hz) * static_cast<float>(msShown) / MS_PER_S;
  return static_cast<int32_t>(std::lround(amplitudePx * std::sin(phase)));
}

// El aviso dura durationMs exactos
inline bool isPopupOver(uint32_t startMs, uint32_t nowMs, uint32_t durationMs) {
  return hasElapsed(startMs, nowMs, durationMs);
}

// Mientras se ve el aviso los botones no llegan a la pantalla de debajo.
// Solo el 4 hace algo: lo cancela, sea corto o largo (mantenido mas de
// medio segundo llega como pulsacion larga y tambien tiene que cortarlo).
inline bool popupCancelsOn(const ButtonEvent& ev) {
  return ev.button == Button::BTN4_BACK;
}

// Que hacer al llegar un mensaje (o en cada fotograma con uno pendiente)
enum class PopupStart : uint8_t { None, Show, Defer };

// Durante el arranque no sale. Con un dialogo abierto (borrar, restaurar...)
// se aplaza hasta que se cierre: el aviso no corta una confirmacion.
inline PopupStart popupStartFor(bool hasArrival, bool isBooting, bool isDialogOpen) {
  if (!hasArrival || isBooting) {
    return PopupStart::None;
  }
  return isDialogOpen ? PopupStart::Defer : PopupStart::Show;
}
