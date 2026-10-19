#include "trash_menu.h"

#include <cstdio>

#include "config.h"
#include "fonts/fonts.h"
#include "menu.h"
#include "trash_menu_rules.h"
#include "ui_style.h"

namespace {
constexpr const char* TITLE = "Papelera";
constexpr const char* FOOTER = "";
constexpr const char* LABELS[] = {"Ver mensajes", "Seleccionar", "Vaciar papelera"};
constexpr size_t LABEL_MAX = 40;

// 4 filas como mucho, igual que el menu principal
constexpr int32_t ROW_TOP = 43;
constexpr int32_t ROW_HEIGHT = 43;
constexpr int32_t ROW_PADDING = 3;
constexpr int32_t ROW_RADIUS = 10;
constexpr int32_t LABEL_X = UI_SIDE_MARGIN + 10;
constexpr int32_t LABEL_MAX_WIDTH = LCD_WIDTH - UI_SIDE_MARGIN - 6 - LABEL_X;
constexpr int32_t FOOTER_Y = 228;
constexpr uint32_t HIGHLIGHT_COLOR = 0x4643D1U;  // RGB888, como el menu
static_assert(ROW_TOP + 4 * ROW_HEIGHT <= FOOTER_Y - 10, "las filas de la papelera pisan el pie");

void drawRow(LGFX_Sprite& canvas, uint8_t row, bool isSelected, const char* label) {
  const int32_t rowY = ROW_TOP + row * ROW_HEIGHT;
  if (isSelected) {
    canvas.fillRoundRect(UI_SIDE_MARGIN, rowY + ROW_PADDING, LCD_WIDTH - 2 * UI_SIDE_MARGIN,
                         ROW_HEIGHT - 2 * ROW_PADDING, ROW_RADIUS, HIGHLIGHT_COLOR);
  }
  canvas.setFont(&FreeSansBold9pt8b);
  canvas.setTextSize(1);
  // "Cancelar borrado pendiente (N)" no cabe en negrita: va en normal
  if (canvas.textWidth(label) > LABEL_MAX_WIDTH) {
    canvas.setFont(&FreeSans9pt8b);
  }
  canvas.setTextColor(isSelected ? UI_TEXT_COLOR : UI_DIM_COLOR);
  canvas.setTextDatum(lgfx::textdatum_t::middle_left);
  canvas.drawString(label, LABEL_X, rowY + ROW_HEIGHT / 2);
}
}  // namespace

void trashMenuDraw(LGFX_Sprite& canvas, uint8_t selected, uint16_t pendingCount, bool isOnline) {
  menuDrawHeader(canvas, TITLE, isOnline);
  const uint8_t sel = trashMenuClamp(selected, pendingCount);
  for (uint8_t i = 0; i < static_cast<uint8_t>(TrashMenuItem::CANCEL_PENDING); ++i) {
    drawRow(canvas, i, i == sel, LABELS[i]);
  }
  if (pendingCount > 0) {
    char label[LABEL_MAX];
    snprintf(label, sizeof(label), "Cancelar borrado pendiente (%u)", static_cast<unsigned>(pendingCount));
    const auto row = static_cast<uint8_t>(TrashMenuItem::CANCEL_PENDING);
    drawRow(canvas, row, row == sel, label);
  }
  menuDrawFooter(canvas, FOOTER, FOOTER_Y);
}
