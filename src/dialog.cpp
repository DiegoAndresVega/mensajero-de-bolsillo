#include "dialog.h"

#include "config.h"
#include "fonts/fonts.h"
#include "ui_style.h"

namespace {
constexpr int32_t BOX_WIDTH = 256;
constexpr int32_t BOX_HEIGHT = 112;
constexpr int32_t BOX_X = (LCD_WIDTH - BOX_WIDTH) / 2;
constexpr int32_t BOX_Y = (LCD_HEIGHT - BOX_HEIGHT) / 2;
constexpr int32_t BOX_RADIUS = 12;
constexpr int32_t BORDER_PX = 2;
constexpr int32_t QUESTION_Y = BOX_Y + 30;
constexpr int32_t DETAIL_Y = BOX_Y + 56;
constexpr int32_t BUTTONS_Y = BOX_Y + BOX_HEIGHT - 22;
constexpr uint32_t BOX_COLOR = 0x636363U;  // RGB888
constexpr uint32_t BORDER_COLOR = UI_ACCENT_PINK;

// Oscurece todo el canvas a la mitad: "semitransparente" sin leer pixeles
// uno a uno. El sprite guarda RGB565 con los bytes cambiados de orden.
void darkenCanvas(LGFX_Sprite& canvas) {
  auto* pixels = static_cast<uint16_t*>(canvas.getBuffer());
  if (pixels == nullptr) {
    return;
  }
  constexpr uint16_t HALF_MASK = 0x7BEF;  // quita el bit bajo de cada canal tras >> 1
  const size_t count = static_cast<size_t>(canvas.width()) * canvas.height();
  for (size_t i = 0; i < count; ++i) {
    const auto rgb = static_cast<uint16_t>((pixels[i] >> 8) | (pixels[i] << 8));
    const auto dark = static_cast<uint16_t>((rgb >> 1) & HALF_MASK);
    pixels[i] = static_cast<uint16_t>((dark >> 8) | (dark << 8));
  }
}

void drawCentered(LGFX_Sprite& canvas, const lgfx::GFXfont& font, uint32_t color, const char* text, int32_t y) {
  canvas.setFont(&font);
  canvas.setTextSize(1);
  canvas.setTextColor(color);
  canvas.setTextDatum(lgfx::textdatum_t::middle_center);
  canvas.drawString(text, LCD_WIDTH / 2, y);
}
}  // namespace

void dialogDraw(LGFX_Sprite& canvas, const DialogText& text, bool isFirstDraw) {
  if (isFirstDraw) {
    darkenCanvas(canvas);
  }
  canvas.fillRoundRect(BOX_X, BOX_Y, BOX_WIDTH, BOX_HEIGHT, BOX_RADIUS, BORDER_COLOR);
  canvas.fillRoundRect(BOX_X + BORDER_PX, BOX_Y + BORDER_PX, BOX_WIDTH - 2 * BORDER_PX, BOX_HEIGHT - 2 * BORDER_PX,
                       BOX_RADIUS - BORDER_PX, BOX_COLOR);
  drawCentered(canvas, FreeSansBold9pt8b, UI_TEXT_COLOR, text.question, QUESTION_Y);
  if (text.detail != nullptr) {
    drawCentered(canvas, FreeSans9pt8b, UI_TEXT_COLOR, text.detail, DETAIL_Y);
  }
  if (text.buttons != nullptr) {
    drawCentered(canvas, FreeSans9pt8b, UI_DIM_COLOR, text.buttons, BUTTONS_Y);
  }
}
