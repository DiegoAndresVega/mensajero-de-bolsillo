#include "notification_popup.h"

#include "config.h"
#include "notifications.h"
#include "ui_style.h"

namespace {
// Tarjeta centrada, con margen de sobra para la vibracion
constexpr int32_t CARD_WIDTH = 176;
constexpr int32_t CARD_HEIGHT = 132;
constexpr int32_t CARD_X = (LCD_WIDTH - CARD_WIDTH) / 2;
constexpr int32_t CARD_Y = (LCD_HEIGHT - CARD_HEIGHT) / 2;
constexpr int32_t CARD_RADIUS = 16;
// Rosa oscuro: el contorno negro del sobre tiene que verse sobre la tarjeta
constexpr uint32_t CARD_PINK_PCT = 45;
constexpr uint32_t CARD_COLOR = uiBlendRgb(UI_ACCENT_PINK, UI_BG_COLOR, CARD_PINK_PCT);
constexpr uint32_t CARD_BORDER_COLOR = UI_ACCENT_PINK;

// Sobre: cuerpo blanco, contorno y solapa negros, punto rojo arriba a la derecha
constexpr int32_t ENVELOPE_WIDTH = 120;
constexpr int32_t ENVELOPE_HEIGHT = 80;
constexpr int32_t ENVELOPE_RADIUS = 6;
constexpr int32_t OUTLINE_PX = 3;
constexpr float FLAP_LINE_WIDTH = 3.0F;
constexpr int32_t BADGE_RADIUS = 13;
constexpr int32_t BADGE_RING_PX = 3;
constexpr uint32_t ENVELOPE_COLOR = 0xFFFFFFU;  // RGB888
constexpr uint32_t OUTLINE_COLOR = 0x000000U;   // RGB888
static_assert(NOTIF_POPUP_WOBBLE_PX + ENVELOPE_WIDTH / 2 + BADGE_RADIUS < CARD_WIDTH / 2,
              "el sobre vibrando se sale de la tarjeta");

void drawEnvelope(LGFX_Sprite& canvas, int32_t cx, int32_t cy) {
  const int32_t x = cx - ENVELOPE_WIDTH / 2;
  const int32_t y = cy - ENVELOPE_HEIGHT / 2;
  // Contorno: rectangulo negro con uno blanco mas pequeno encima
  canvas.fillRoundRect(x, y, ENVELOPE_WIDTH, ENVELOPE_HEIGHT, ENVELOPE_RADIUS, OUTLINE_COLOR);
  canvas.fillRoundRect(x + OUTLINE_PX, y + OUTLINE_PX, ENVELOPE_WIDTH - 2 * OUTLINE_PX,
                       ENVELOPE_HEIGHT - 2 * OUTLINE_PX, ENVELOPE_RADIUS - 2, ENVELOPE_COLOR);
  // Solapa: una V de las esquinas de arriba hasta algo mas de la mitad
  const int32_t tipY = y + ENVELOPE_HEIGHT * 11 / 20;
  canvas.drawWideLine(x + OUTLINE_PX, y + OUTLINE_PX, cx, tipY, FLAP_LINE_WIDTH, OUTLINE_COLOR);
  canvas.drawWideLine(x + ENVELOPE_WIDTH - 1 - OUTLINE_PX, y + OUTLINE_PX, cx, tipY, FLAP_LINE_WIDTH,
                      OUTLINE_COLOR);
  // Punto rojo de "nuevo" con un aro del color de la tarjeta
  const int32_t badgeX = x + ENVELOPE_WIDTH;
  canvas.fillCircle(badgeX, y, BADGE_RADIUS + BADGE_RING_PX, CARD_COLOR);
  canvas.fillCircle(badgeX, y, BADGE_RADIUS, NOTIFICATION_BADGE_COLOR);
}
}  // namespace

ScreenRect notificationPopupRect() {
  return {CARD_X, CARD_Y, CARD_WIDTH, CARD_HEIGHT};
}

void notificationPopupDraw(LGFX_Sprite& canvas, int32_t offsetX) {
  canvas.fillRoundRect(CARD_X, CARD_Y, CARD_WIDTH, CARD_HEIGHT, CARD_RADIUS, CARD_BORDER_COLOR);
  canvas.fillRoundRect(CARD_X + 2, CARD_Y + 2, CARD_WIDTH - 4, CARD_HEIGHT - 4, CARD_RADIUS - 2, CARD_COLOR);
  // Un poco por debajo del centro: el punto rojo sobresale por arriba
  drawEnvelope(canvas, LCD_WIDTH / 2 + offsetX, LCD_HEIGHT / 2 + BADGE_RADIUS / 2);
}
