#include "busy_icon.h"

#include "config.h"
#include "display.h"

namespace {
constexpr uint8_t COLOR_DEPTH = 16;
constexpr int32_t ICON_SIZE = 36;
constexpr int32_t ICON_MARGIN = 6;
constexpr int32_t ICON_X = LCD_WIDTH - ICON_SIZE - ICON_MARGIN;
constexpr int32_t ICON_Y = ICON_MARGIN;
constexpr uint32_t FRAME_COLOR = 0xFFFFFFU;  // RGB888
constexpr uint32_t SAND_COLOR = 0xF5B942U;   // RGB888

LGFX_Sprite icon;

// Reloj de arena: barras arriba y abajo, dos triangulos de cristal y arena
// en cada mitad
void drawIcon() {
  const int32_t s = ICON_SIZE;
  const int32_t mid = s / 2;
  const int32_t bar = s / 9;
  const int32_t inset = s / 5;
  icon.fillScreen(EYE_BG_COLOR);
  icon.fillRoundRect(inset / 2, 0, s - inset, bar, bar / 2, FRAME_COLOR);
  icon.fillRoundRect(inset / 2, s - bar, s - inset, bar, bar / 2, FRAME_COLOR);
  icon.drawTriangle(inset, bar, s - 1 - inset, bar, mid, mid, FRAME_COLOR);
  icon.drawTriangle(mid, mid, inset, s - 1 - bar, s - 1 - inset, s - 1 - bar, FRAME_COLOR);
  // Poca arena arriba, montoncito abajo
  icon.fillTriangle(inset + 5, bar + 4, s - 1 - inset - 5, bar + 4, mid, mid - 4, SAND_COLOR);
  icon.fillTriangle(mid, mid + 6, inset + 3, s - 2 - bar, s - 1 - inset - 3, s - 2 - bar, SAND_COLOR);
}
}  // namespace

bool busyIconInit() {
  icon.setColorDepth(COLOR_DEPTH);
  if (icon.createSprite(ICON_SIZE, ICON_SIZE) == nullptr) {
    return false;
  }
  drawIcon();
  return true;
}

void busyIconPush() {
  if (icon.getBuffer() != nullptr) {
    icon.pushSprite(&display(), ICON_X, ICON_Y);
  }
}

ScreenRect busyIconRect() {
  return {ICON_X, ICON_Y, ICON_SIZE, ICON_SIZE};
}
