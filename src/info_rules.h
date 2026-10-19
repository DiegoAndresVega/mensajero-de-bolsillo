#pragma once

// Logica pura de la pantalla Info del menu principal (mensaje personal del
// propietario, personal_message de config.json), sin dependencias de
// Arduino: se compila tambien en el entorno native para los tests (pio test
// -e native). Dibujar es cosa de info_screen.

#include <cstddef>
#include <cstdint>
#include <cstring>

#include "button_rules.h"
#include "message_rules.h"

// ----- Botones ------------------------------------------------------------------

struct InfoStep {
  uint16_t topLine;
  bool isBack;
};

// 1/2 hacen scroll de `step` lineas sin pasarse del final, 4 vuelve al menu.
// El resto no hace nada. Mismo scroll que el visor de mensajes.
inline InfoStep infoOnButton(uint16_t topLine, const ButtonEvent& ev, uint16_t totalLines, uint16_t visibleLines,
                             uint16_t step) {
  if (ev.button == Button::BTN3_OK) {
    return {topLine, false};
  }
  const MessageViewStep view = messageViewOnButton(topLine, ev, totalLines, visibleLines, step);
  return {view.topLine, view.isBack};
}

// ----- Mensaje personal -------------------------------------------------------------

// Texto de Info: el mensaje de config.json pasado a Latin-1 (como los
// mensajes) y cortado a maxChars con "...". Vacio o solo espacios: `fallback`.
inline void formatPersonalMessage(const char* utf8, size_t maxChars, const char* fallback, char* out,
                                  size_t outSize) {
  if (outSize == 0) {
    return;
  }
  // Latin-1 ocupa como mucho lo mismo que el UTF-8 de origen: basta con outSize
  toDisplayLatin1(utf8, out, outSize);
  bool isBlank = true;
  for (const char* p = out; *p != '\0'; ++p) {
    if (*p != ' ' && *p != '\n' && *p != '\t' && *p != '\r') {
      isBlank = false;
      break;
    }
  }
  if (isBlank) {
    truncateForRow(fallback, maxChars, out, outSize);
    return;
  }
  // Corte en el sitio, con las mismas reglas que truncateForRow ("..." dentro
  // de maxChars, sin espacios antes)
  constexpr size_t ELLIPSIS_LEN = 3;
  const size_t len = strlen(out);
  if (len <= maxChars) {
    return;
  }
  if (maxChars <= ELLIPSIS_LEN) {
    out[maxChars] = '\0';
    return;
  }
  size_t keep = maxChars - ELLIPSIS_LEN;
  while (keep > 0 && out[keep - 1] == ' ') {
    --keep;
  }
  memcpy(out + keep, "...", ELLIPSIS_LEN + 1);
}
