#include "info_screen.h"

#include <algorithm>

#include "config.h"
#include "fonts/fonts.h"
#include "info_rules.h"
#include "menu.h"
#include "ui_style.h"

namespace {
constexpr const char* TITLE = "Info";
// Se ve si config.json no trae personal_message. En Latin-1 (\xED = i con tilde)
constexpr const char* DEFAULT_MESSAGE =
    "Aqu\xED va tu mensaje. Escr\xEDbelo en \"personal_message\" dentro de config.json, en la ra\xEDz de "
    "la tarjeta SD, y se ver\xE1 en esta pantalla al reiniciar.";
constexpr const char* SCROLL_FOOTER = "";
constexpr const char* BACK_FOOTER = "";

// Texto con scroll entre la cabecera y el pie
constexpr int32_t TEXT_TOP = 50;
constexpr int32_t TEXT_BOTTOM = 214;
constexpr int32_t TEXT_X = UI_SIDE_MARGIN;
constexpr int32_t SCROLLBAR_WIDTH = 4;
constexpr int32_t SCROLLBAR_X = LCD_WIDTH - SCROLLBAR_WIDTH - 4;
constexpr int32_t SCROLLBAR_MIN_LEN = 12;
constexpr int32_t TEXT_WIDTH = SCROLLBAR_X - 6 - TEXT_X;
constexpr int32_t LINE_HEIGHT = 20;
constexpr int32_t FOOTER_Y = 228;
constexpr uint16_t MAX_LINES = 64;

char text[INFO_MAX_CHARS + 1] = "";
TextLine lines[MAX_LINES];
uint16_t lineCount = 0;

void drawScrollbar(LGFX_Sprite& canvas, uint16_t top) {
  const uint16_t visible = infoScreenVisibleLines();
  if (lineCount <= visible) {
    return;
  }
  const int32_t trackLen = TEXT_BOTTOM - TEXT_TOP;
  const ScrollThumb thumb = scrollThumb(lineCount, visible, top, trackLen, SCROLLBAR_MIN_LEN);
  canvas.fillRoundRect(SCROLLBAR_X, TEXT_TOP, SCROLLBAR_WIDTH, trackLen, SCROLLBAR_WIDTH / 2, UI_SEPARATOR_COLOR);
  canvas.fillRoundRect(SCROLLBAR_X, TEXT_TOP + thumb.offset, SCROLLBAR_WIDTH, thumb.length, SCROLLBAR_WIDTH / 2,
                       UI_DIM_COLOR);
}
}  // namespace

void infoScreenInit(LGFX_Sprite& canvas, const char* personalMessageUtf8) {
  formatPersonalMessage(personalMessageUtf8, INFO_MAX_CHARS, DEFAULT_MESSAGE, text, sizeof(text));
  canvas.setFont(&FreeSans9pt8b);
  canvas.setTextSize(1);
  // Se mide cada trozo con la fuente del canvas: son INFO_MAX_CHARS
  // caracteres como mucho y solo una vez al arrancar
  auto measure = [&canvas](const char* s, size_t len) {
    char buf[INFO_MAX_CHARS + 1];
    const size_t n = std::min(len, sizeof(buf) - 1);
    memcpy(buf, s, n);
    buf[n] = '\0';
    return canvas.textWidth(buf);
  };
  lineCount = wrapText(text, TEXT_WIDTH, measure, lines, MAX_LINES);
}

uint16_t infoScreenLineCount() {
  return lineCount;
}

uint16_t infoScreenVisibleLines() {
  return static_cast<uint16_t>((TEXT_BOTTOM - TEXT_TOP) / LINE_HEIGHT);
}

void infoScreenDraw(LGFX_Sprite& canvas, uint16_t topLine, bool isOnline) {
  menuDrawHeader(canvas, TITLE, isOnline);
  canvas.setFont(&FreeSans9pt8b);
  canvas.setTextSize(1);
  canvas.setTextColor(UI_TEXT_COLOR);
  canvas.setTextDatum(lgfx::textdatum_t::top_left);
  const uint16_t last = std::min<uint16_t>(lineCount, topLine + infoScreenVisibleLines());
  // Una linea nunca pasa de TEXT_WIDTH pixeles: con 1 px por caracter como
  // minimo, cabe en este buffer
  char line[TEXT_WIDTH + 1];
  for (uint16_t i = topLine; i < last; ++i) {
    const size_t len = std::min<size_t>(lines[i].length, sizeof(line) - 1);
    memcpy(line, text + lines[i].start, len);
    line[len] = '\0';
    canvas.drawString(line, TEXT_X, TEXT_TOP + (i - topLine) * LINE_HEIGHT);
  }
  drawScrollbar(canvas, topLine);
  menuDrawFooter(canvas, lineCount > infoScreenVisibleLines() ? SCROLL_FOOTER : BACK_FOOTER, FOOTER_Y);
}
