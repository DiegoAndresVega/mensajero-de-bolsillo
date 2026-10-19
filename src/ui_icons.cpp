#include "ui_icons.h"

#include "config.h"
#include "ui_style.h"

namespace {
// Grosor del contorno del corazon vacio
constexpr int32_t HEART_OUTLINE_PX = 2;
// Centro del corazon de favorito: a la altura del titulo de la cabecera
constexpr int32_t FAVORITE_X = LCD_WIDTH - UI_SIDE_MARGIN - FAVORITE_ICON_SIZE / 2;
constexpr int32_t FAVORITE_Y = UI_TITLE_Y;
}  // namespace

void uiDrawHeart(LGFX_Sprite& canvas, int32_t cx, int32_t cy, int32_t size, uint32_t color) {
  const int32_t r = size / 4;
  const int32_t lobeY = cy - r / 2;
  canvas.fillCircle(cx - r, lobeY, r, color);
  canvas.fillCircle(cx + r, lobeY, r, color);
  const int32_t baseY = lobeY + r / 3;
  const int32_t tipY = cy + size / 2 - r / 2;
  canvas.fillTriangle(cx - 2 * r, baseY, cx + 2 * r, baseY, cx, tipY, color);
}

void uiDrawHeartOutline(LGFX_Sprite& canvas, int32_t cx, int32_t cy, int32_t size, uint32_t color,
                        uint32_t bgColor) {
  uiDrawHeart(canvas, cx, cy, size, color);
  // El de dentro baja un poco: la punta es mas fina que los lobulos
  uiDrawHeart(canvas, cx, cy - 1, size - 3 * HEART_OUTLINE_PX, bgColor);
}

void uiDrawFavoriteBadge(LGFX_Sprite& canvas, bool isFavorite) {
  if (isFavorite) {
    uiDrawHeart(canvas, FAVORITE_X, FAVORITE_Y, FAVORITE_ICON_SIZE, UI_ACCENT_PINK);
  } else {
    uiDrawHeartOutline(canvas, FAVORITE_X, FAVORITE_Y, FAVORITE_ICON_SIZE, UI_DIM_COLOR, UI_BG_COLOR);
  }
}

void uiDrawInfoIcon(LGFX_Sprite& canvas, int32_t cx, int32_t cy, int32_t size, uint32_t color, uint32_t bgColor) {
  const int32_t radius = size / 2;
  const int32_t stemWidth = size / 7;
  canvas.fillCircle(cx, cy, radius, color);
  canvas.fillCircle(cx, cy - radius / 2, stemWidth * 2 / 3, bgColor);
  canvas.fillRect(cx - stemWidth / 2, cy - radius / 5, stemWidth, radius * 4 / 5, bgColor);
}

void uiDrawPhotoIcon(LGFX_Sprite& canvas, int32_t cx, int32_t cy, int32_t size, uint32_t color, uint32_t bgColor) {
  const int32_t w = size;
  const int32_t h = size * 4 / 5;
  const int32_t left = cx - w / 2;
  const int32_t top = cy - h / 2;
  const int32_t bottom = top + h - 1;
  const int32_t border = size / 10 + 1;
  canvas.fillRoundRect(left, top, w, h, border, color);
  // Dentro, del color de fondo: un sol arriba a la derecha y dos montanas
  canvas.fillCircle(left + w * 2 / 3, top + h / 3, size / 10 + 1, bgColor);
  canvas.fillTriangle(left + border, bottom - border, left + w * 2 / 5, top + h * 2 / 5, left + w * 3 / 4,
                      bottom - border, bgColor);
  canvas.fillTriangle(left + w / 2, bottom - border, left + w * 7 / 10, top + h * 3 / 5, left + w - border,
                      bottom - border, bgColor);
}

void uiDrawTrashIcon(LGFX_Sprite& canvas, int32_t cx, int32_t cy, int32_t size, uint32_t color, uint32_t bgColor) {
  const int32_t half = size / 2;
  const int32_t lidH = size / 8 + 1;
  const int32_t lidY = cy - half + lidH;
  // Asa, tapa y cubo algo mas estrecho abajo
  canvas.fillRect(cx - size / 6, cy - half, size / 3, lidH, color);
  canvas.fillRect(cx - half, lidY, size, lidH, color);
  const int32_t bodyTop = lidY + lidH + 1;
  const int32_t bodyBottom = cy + half;
  const int32_t topHalfW = half * 4 / 5;
  const int32_t bottomHalfW = half * 3 / 5;
  canvas.fillTriangle(cx - topHalfW, bodyTop, cx + topHalfW, bodyTop, cx - bottomHalfW, bodyBottom, color);
  canvas.fillTriangle(cx + topHalfW, bodyTop, cx + bottomHalfW, bodyBottom, cx - bottomHalfW, bodyBottom, color);
  // Tres ranuras verticales
  const int32_t slotW = size / 12 + 1;
  const int32_t slotTop = bodyTop + size / 8;
  const int32_t slotH = bodyBottom - slotTop - size / 8;
  const int32_t gap = bottomHalfW * 2 / 3;
  for (int32_t dx = -gap; dx <= gap; dx += gap) {
    canvas.fillRect(cx + dx - slotW / 2, slotTop, slotW, slotH, bgColor);
  }
}

void uiDrawSpeaker(LGFX_Sprite& canvas, int32_t x, int32_t y, int32_t w, int32_t h, uint32_t color) {
  const int32_t bodyW = w / 3;
  const int32_t bodyH = h / 2;
  const int32_t cy = y + h / 2;
  canvas.fillRect(x, cy - bodyH / 2, bodyW, bodyH, color);
  const int32_t coneLeft = x + bodyW;
  const int32_t coneRight = x + w * 3 / 4;
  canvas.fillTriangle(coneLeft, cy - bodyH / 2, coneRight, y, coneRight, y + h - 1, color);
  canvas.fillTriangle(coneLeft, cy - bodyH / 2, coneLeft, cy + bodyH / 2, coneRight, y + h - 1, color);
}
