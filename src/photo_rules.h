#pragma once

// Logica pura de la pantalla Fotos (las fotos que se copian a mano a /fotos
// de la SD), sin dependencias de Arduino: se compila tambien en el entorno
// native para los tests (pio test -e native). Leer y dibujar es cosa de photos.

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <initializer_list>

#include "button_rules.h"

// ----- Ficheros -------------------------------------------------------------------

// Un fichero de /fotos que se muestra: .jpg o .jpeg (en mayusculas o
// minusculas), sin "." delante (los "._" que deja el Finder de macOS) y con
// maxLen caracteres como mucho.
inline bool isPhotoFileName(const char* name, size_t maxLen) {
  const size_t len = strlen(name);
  if (len == 0 || len > maxLen || name[0] == '.') {
    return false;
  }
  for (const char* ext : {".jpg", ".jpeg"}) {
    const size_t extLen = strlen(ext);
    if (len <= extLen) {
      continue;
    }
    bool isMatch = true;
    for (size_t i = 0; i < extLen; ++i) {
      char c = name[len - extLen + i];
      if (c >= 'A' && c <= 'Z') {
        c = static_cast<char>(c - 'A' + 'a');
      }
      if (c != ext[i]) {
        isMatch = false;
        break;
      }
    }
    if (isMatch) {
      return true;
    }
  }
  return false;
}

// ----- Botones ------------------------------------------------------------------

struct PhotoStep {
  uint16_t index;
  bool isBack;
};

inline uint16_t photosClampIndex(uint16_t index, uint16_t count) {
  return index < count ? index : 0;
}

// 1 foto anterior y 2 siguiente, con vuelta (como el menu); 4 vuelve al menu.
// El 3 y las pulsaciones largas no hacen nada.
inline PhotoStep photosOnButton(uint16_t index, const ButtonEvent& ev, uint16_t count) {
  if (ev.longPress) {
    return {index, false};
  }
  switch (ev.button) {
    case Button::BTN4_BACK:
      return {index, true};
    case Button::BTN1_UP:
      return {count == 0 ? index : static_cast<uint16_t>((index + count - 1) % count), false};
    case Button::BTN2_DOWN:
      return {count == 0 ? index : static_cast<uint16_t>((index + 1) % count), false};
    default:
      return {index, false};
  }
}
