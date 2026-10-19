#pragma once

#include <cstdint>

// Colores y medidas comunes de las pantallas con cabecera (menu, lista y
// visor de mensajes). Colores en RGB888.
constexpr uint32_t UI_BG_COLOR = 0xE88BE2U;
constexpr uint32_t UI_TEXT_COLOR = 0x000000U;
constexpr uint32_t UI_DIM_COLOR = 0x000000U;
constexpr uint32_t UI_SEPARATOR_COLOR = 0x4388D1U;

// Cabecera: titulo centrado y linea separadora debajo
constexpr int32_t UI_TITLE_Y = 20;
constexpr int32_t UI_SEPARATOR_Y = 40;
constexpr int32_t UI_SIDE_MARGIN = 12;

// Rosa de la seleccion de la lista, los favoritos y la onda del audio
constexpr uint32_t UI_ACCENT_PINK = 0xFF0000U;

constexpr uint32_t uiBlendChannel(uint32_t fg, uint32_t bg, uint32_t shift, uint32_t pct) {
  return ((((fg >> shift) & 0xFFU) * pct + ((bg >> shift) & 0xFFU) * (100 - pct)) / 100) << shift;
}

// Mezcla de dos colores RGB888: pct % de fg sobre bg
constexpr uint32_t uiBlendRgb(uint32_t fg, uint32_t bg, uint32_t pct) {
  return uiBlendChannel(fg, bg, 16, pct) | uiBlendChannel(fg, bg, 8, pct) | uiBlendChannel(fg, bg, 0, pct);
}
