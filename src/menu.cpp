#include "menu.h"

#include <algorithm>
#include <cmath>

#include "config.h"
#include "fonts/fonts.h"
#include "notifications.h"
#include "ui_icons.h"
#include "ui_style.h"

namespace {
// Colores RGB888 (los comunes en ui_style.h)
constexpr uint32_t BG_COLOR = UI_BG_COLOR;
constexpr uint32_t TEXT_COLOR = UI_TEXT_COLOR;
constexpr uint32_t DIM_COLOR = UI_DIM_COLOR;
constexpr uint32_t HIGHLIGHT_COLOR = 0x4643D1U;

// Layout apaisado de 280x240: cabecera y 6 filas (icono + nombre + contador).
// El pie del menu esta vacio: las filas llegan casi al borde de abajo.
constexpr int32_t SIDE_MARGIN = UI_SIDE_MARGIN;
constexpr int32_t ROW_TOP = 43;
constexpr int32_t ROW_HEIGHT = 32;
constexpr int32_t ROW_PADDING = 3;
constexpr int32_t ROW_RADIUS = 10;
constexpr int32_t ICON_CENTER_X = 44;
constexpr int32_t ICON_SIZE = 24;
constexpr int32_t LABEL_X = 84;
// Contador de no leidos: pastilla roja alineada a la derecha de la fila
constexpr int32_t COUNTER_RIGHT_X = LCD_WIDTH - SIDE_MARGIN - 12;
constexpr int32_t COUNTER_HEIGHT = 26;
constexpr int32_t COUNTER_PADDING_X = 9;
constexpr uint32_t COUNTER_MAX_SHOWN = 99;
constexpr int32_t BODY_TOP = 66;
constexpr int32_t BODY_LINE_HEIGHT = 34;
constexpr int32_t BOTTOM_MARGIN = 4;
static_assert(ROW_TOP + MENU_ITEM_COUNT * ROW_HEIGHT <= LCD_HEIGHT - BOTTOM_MARGIN, "las filas del menu no caben");

constexpr const char* ITEM_LABELS[MENU_ITEM_COUNT] = {"Mensajes", "Favoritos", "Fotos", "Papelera", "Ajustes", "Info"};

constexpr uint8_t GEAR_TOOTH_PAIRS = 4;
constexpr float PI_F = 3.1415927F;
constexpr size_t COUNTER_TEXT_MAX = 8;
// Icono de "sin WiFi" en la esquina superior derecha de la cabecera
constexpr int32_t OFFLINE_ICON_SIZE = 22;
constexpr int32_t OFFLINE_ICON_X = LCD_WIDTH - UI_SIDE_MARGIN - OFFLINE_ICON_SIZE / 2;
constexpr int32_t OFFLINE_ICON_LEFT_X = UI_SIDE_MARGIN + OFFLINE_ICON_SIZE / 2;

// ----- Iconos -----------------------------------------------------------------

// Rueda dentada: 4 barras redondeadas cruzadas (8 dientes) + disco con agujero
void drawGearIcon(LGFX_Sprite& canvas, int32_t cx, int32_t cy, int32_t size, uint32_t color, uint32_t bg) {
  const int32_t outer = size / 2;
  for (uint8_t i = 0; i < GEAR_TOOTH_PAIRS; ++i) {
    const float angle = PI_F * i / GEAR_TOOTH_PAIRS;
    const int32_t dx = static_cast<int32_t>(std::lround(outer * std::cos(angle)));
    const int32_t dy = static_cast<int32_t>(std::lround(outer * std::sin(angle)));
    canvas.drawWideLine(cx - dx, cy - dy, cx + dx, cy + dy, size / 10.0F, color);
  }
  canvas.fillCircle(cx, cy, outer * 3 / 4, color);
  canvas.fillCircle(cx, cy, outer * 3 / 10, bg);
}

void drawItemIcon(LGFX_Sprite& canvas, MenuItem item, int32_t cy, uint32_t color, uint32_t bg, bool hasBadge) {
  switch (item) {
    case MenuItem::MESSAGES: {
      const int32_t h = ICON_SIZE * 7 / 10;
      notificationsDrawIcon(canvas, ICON_CENTER_X - ICON_SIZE / 2, cy - h / 2, ICON_SIZE, h, color, bg, hasBadge);
      return;
    }
    case MenuItem::FAVORITES:
      uiDrawHeart(canvas, ICON_CENTER_X, cy, ICON_SIZE, color);
      return;
    case MenuItem::PHOTOS:
      uiDrawPhotoIcon(canvas, ICON_CENTER_X, cy, ICON_SIZE, color, bg);
      return;
    case MenuItem::TRASH:
      uiDrawTrashIcon(canvas, ICON_CENTER_X, cy, ICON_SIZE, color, bg);
      return;
    case MenuItem::SETTINGS:
      drawGearIcon(canvas, ICON_CENTER_X, cy, ICON_SIZE, color, bg);
      return;
    case MenuItem::INFO:
      uiDrawInfoIcon(canvas, ICON_CENTER_X, cy, ICON_SIZE, color, bg);
      return;
  }
}

// Pastilla roja con el numero de no leidos (99+ si hay mas)
void drawUnreadCounter(LGFX_Sprite& canvas, int32_t cy, uint32_t unread) {
  char text[COUNTER_TEXT_MAX];
  if (unread > COUNTER_MAX_SHOWN) {
    snprintf(text, sizeof(text), "%lu+", static_cast<unsigned long>(COUNTER_MAX_SHOWN));
  } else {
    snprintf(text, sizeof(text), "%lu", static_cast<unsigned long>(unread));
  }
  canvas.setFont(&FreeSansBold9pt8b);
  const int32_t width = std::max<int32_t>(COUNTER_HEIGHT, canvas.textWidth(text) + 2 * COUNTER_PADDING_X);
  const int32_t left = COUNTER_RIGHT_X - width;
  canvas.fillRoundRect(left, cy - COUNTER_HEIGHT / 2, width, COUNTER_HEIGHT, COUNTER_HEIGHT / 2,
                       NOTIFICATION_BADGE_COLOR);
  canvas.setTextDatum(lgfx::textdatum_t::middle_center);
  canvas.setTextColor(TEXT_COLOR);
  canvas.drawString(text, left + width / 2, cy);
}

// ----- Pantallas ------------------------------------------------------------------

void drawList(LGFX_Sprite& canvas, const MenuModel& model, uint32_t unread, bool isOnline) {
  menuDrawHeader(canvas, "Menu", isOnline);
  for (uint8_t i = 0; i < MENU_ITEM_COUNT; ++i) {
    const int32_t rowY = ROW_TOP + i * ROW_HEIGHT;
    const int32_t centerY = rowY + ROW_HEIGHT / 2;
    const bool isSelected = i == model.selected;
    const uint32_t rowBg = isSelected ? HIGHLIGHT_COLOR : BG_COLOR;
    const uint32_t fg = isSelected ? TEXT_COLOR : DIM_COLOR;
    if (isSelected) {
      canvas.fillRoundRect(SIDE_MARGIN, rowY + ROW_PADDING, LCD_WIDTH - 2 * SIDE_MARGIN, ROW_HEIGHT - 2 * ROW_PADDING,
                           ROW_RADIUS, rowBg);
    }
    const auto item = static_cast<MenuItem>(i);
    drawItemIcon(canvas, item, centerY, fg, rowBg, item == MenuItem::MESSAGES && unread > 0);
    canvas.setTextDatum(lgfx::textdatum_t::middle_left);
    canvas.setFont(&FreeSansBold12pt8b);
    canvas.setTextColor(fg);
    canvas.drawString(ITEM_LABELS[i], LABEL_X, centerY);
    if (item == MenuItem::MESSAGES && unread > 0) {
      drawUnreadCounter(canvas, centerY, unread);
    }
  }
}

}  // namespace

void menuDraw(LGFX_Sprite& canvas, const MenuModel& model, uint32_t unread, bool isOnline) {
  drawList(canvas, model, unread, isOnline);
}

void menuDrawHeader(LGFX_Sprite& canvas, const char* title, bool isOnline, OfflineIconSide side) {
  canvas.fillScreen(BG_COLOR);
  canvas.setTextSize(1);
  canvas.setTextDatum(lgfx::textdatum_t::middle_center);
  canvas.setFont(&FreeSansBold12pt8b);
  canvas.setTextColor(TEXT_COLOR);
  canvas.drawString(title, LCD_WIDTH / 2, UI_TITLE_Y);
  canvas.drawFastHLine(SIDE_MARGIN, UI_SEPARATOR_Y, LCD_WIDTH - 2 * SIDE_MARGIN, UI_SEPARATOR_COLOR);
  if (!isOnline) {
    const int32_t x = side == OfflineIconSide::Right ? OFFLINE_ICON_X : OFFLINE_ICON_LEFT_X;
    notificationsDrawOfflineIcon(canvas, x, UI_TITLE_Y, OFFLINE_ICON_SIZE, DIM_COLOR, BG_COLOR);
  }
}

void menuDrawFooter(LGFX_Sprite& canvas, const char* text, int32_t footerY) {
  canvas.setTextSize(1);
  canvas.setTextDatum(lgfx::textdatum_t::middle_center);
  canvas.setFont(&FreeSans9pt8b);
  canvas.setTextColor(DIM_COLOR);
  canvas.drawString(text, LCD_WIDTH / 2, footerY);
}

void menuDrawBodyLine(LGFX_Sprite& canvas, uint8_t line, const char* label, const char* value) {
  const int32_t y = BODY_TOP + line * BODY_LINE_HEIGHT;
  canvas.setTextSize(1);
  canvas.setTextDatum(lgfx::textdatum_t::middle_left);
  canvas.setFont(&FreeSans9pt8b);
  canvas.setTextColor(DIM_COLOR);
  canvas.drawString(label, SIDE_MARGIN * 2, y);
  canvas.setTextColor(TEXT_COLOR);
  canvas.drawString(value, LCD_WIDTH / 2 - SIDE_MARGIN, y);
}
