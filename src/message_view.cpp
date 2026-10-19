#include "message_view.h"

#include <algorithm>
#include <cmath>

#include "config.h"
#include "fonts/fonts.h"
#include "log.h"
#include "menu.h"
#include "storage.h"
#include "trash_rules.h"
#include "ui_icons.h"
#include "ui_style.h"

namespace {
constexpr const char* TAG = "MSG";
constexpr const char* LOAD_ERROR_TEXT = "No se pudo leer el mensaje.";
constexpr const char* FOOTER_SCROLL = "";
constexpr const char* FOOTER_BACK = "";
constexpr const char* FOOTER_TRASH_SCROLL = "";
constexpr const char* FOOTER_TRASH_BACK = "";
constexpr size_t DATE_TEXT_MAX = 24;
// El titulo se lee entero del JSON (el resumen de la lista lo corta): la web
// admite 100 caracteres
constexpr size_t TITLE_MAX_CHARS = 100;
// Lineas como mucho: titulo + MSG_CONTENT_MAX_CHARS entre ~25 caracteres por
// linea, con margen para los saltos de linea
constexpr uint16_t MAX_LINES = 160;

// Layout de 280x240: cabecera con el autor (0-40), fecha, zona con scroll
// (titulo en negrita, linea en blanco y contenido) y pie
constexpr int32_t SUBTITLE_Y = 52;
constexpr int32_t CONTENT_SEPARATOR_Y = 64;
constexpr int32_t CONTENT_TOP = 70;
constexpr int32_t CONTENT_BOTTOM = 218;
constexpr int32_t CONTENT_X = UI_SIDE_MARGIN;
constexpr int32_t SCROLLBAR_WIDTH = 4;
constexpr int32_t SCROLLBAR_X = LCD_WIDTH - SCROLLBAR_WIDTH - 4;
constexpr int32_t SCROLLBAR_MIN_LEN = 12;
constexpr int32_t CONTENT_WIDTH = SCROLLBAR_X - 6 - CONTENT_X;
constexpr int32_t FOOTER_Y = 229;
// Alto de linea de FreeSans 9 pt a escala 1
constexpr int32_t BASE_LINE_HEIGHT = 20;
// Texto en Latin-1: un byte por caracter, de ' ' a 0xFF
constexpr uint8_t FIRST_PRINTABLE = 0x20;
constexpr uint8_t LAST_PRINTABLE = 0xFF;
constexpr size_t PRINTABLE_COUNT = LAST_PRINTABLE - FIRST_PRINTABLE + 1;

MessageSummary current;
bool isCurrentInTrash = false;
char title[TITLE_MAX_CHARS + 1] = "";
char content[MSG_CONTENT_MAX_CHARS + 1] = "";
// Las titleLineCount primeras lineas son del titulo; el resto, del contenido
TextLine lines[MAX_LINES];
uint16_t lineCount = 0;
uint16_t titleLineCount = 0;

int32_t lineHeight() {
  return static_cast<int32_t>(std::lround(BASE_LINE_HEIGHT * MSG_ROW_FONT_SIZE));
}

void setContentFont(LGFX_Sprite& canvas) {
  canvas.setFont(&FreeSans9pt8b);
  canvas.setTextSize(MSG_ROW_FONT_SIZE);
}

void setTitleFont(LGFX_Sprite& canvas) {
  canvas.setFont(&FreeSansBold9pt8b);
  canvas.setTextSize(MSG_ROW_FONT_SIZE);
}

// Titulo entero y contenido. Si el JSON no se puede leer, el titulo del resumen.
void loadTexts(const MessageSummary& msg) {
  strlcpy(title, msg.titulo, sizeof(title));
  JsonDocument doc;
  char trashPath[TRASH_PATH_SIZE];
  buildMessageFilePath(TRASH_MESSAGES_DIR, msg.id, ".json", trashPath, sizeof(trashPath));
  const String path = isCurrentInTrash ? String(trashPath) : messagePath(msg.id);
  if (!storageReadJson(path.c_str(), doc)) {
    logMsg(TAG, "no se pudo leer %s", msg.id);
    strlcpy(content, LOAD_ERROR_TEXT, sizeof(content));
    return;
  }
  toDisplayLatin1(doc["titulo"] | "", title, sizeof(title));
  toDisplayLatin1(doc["contenido"] | "", content, sizeof(content));
}

// Ancho en pixeles de cada caracter Latin-1 con la fuente activa del canvas
void measureChars(LGFX_Sprite& canvas, int32_t (&widths)[PRINTABLE_COUNT]) {
  char one[2] = {0, 0};
  for (size_t i = 0; i < PRINTABLE_COUNT; ++i) {
    one[0] = static_cast<char>(FIRST_PRINTABLE + i);
    widths[i] = canvas.textWidth(one);
  }
}

// Las fuentes FreeSans no tienen kerning: el ancho de un texto es la suma
// del de sus caracteres. Se mide cada uno una vez por apertura.
auto widthMeasure(const int32_t (&widths)[PRINTABLE_COUNT]) {
  return [&widths](const char* s, size_t len) {
    int32_t total = 0;
    for (size_t i = 0; i < len; ++i) {
      const auto c = static_cast<uint8_t>(s[i]);
      total += c >= FIRST_PRINTABLE ? widths[c - FIRST_PRINTABLE] : 0;
    }
    return total;
  };
}

// Titulo en negrita partido a todo el ancho, una linea en blanco y el contenido
void wrapTexts(LGFX_Sprite& canvas) {
  int32_t boldWidths[PRINTABLE_COUNT];
  int32_t widths[PRINTABLE_COUNT];
  setTitleFont(canvas);
  measureChars(canvas, boldWidths);
  setContentFont(canvas);
  measureChars(canvas, widths);
  titleLineCount = wrapText(title, CONTENT_WIDTH, widthMeasure(boldWidths), lines, MAX_LINES);
  lineCount = titleLineCount;
  if (titleLineCount > 0 && lineCount < MAX_LINES) {
    lines[lineCount++] = {0, 0};
  }
  lineCount += wrapText(content, CONTENT_WIDTH, widthMeasure(widths), lines + lineCount,
                        static_cast<uint16_t>(MAX_LINES - lineCount));
}

// Fecha a la derecha, como antes; el titulo va en la zona con scroll
void drawSubtitle(LGFX_Sprite& canvas) {
  char date[DATE_TEXT_MAX];
  formatDate(current.fecha, true, date, sizeof(date));
  canvas.setTextSize(1);
  canvas.setFont(&FreeSans9pt8b);
  canvas.setTextColor(UI_DIM_COLOR);
  canvas.setTextDatum(lgfx::textdatum_t::middle_right);
  canvas.drawString(date, LCD_WIDTH - UI_SIDE_MARGIN, SUBTITLE_Y);
  canvas.drawFastHLine(UI_SIDE_MARGIN, CONTENT_SEPARATOR_Y, LCD_WIDTH - 2 * UI_SIDE_MARGIN, UI_SEPARATOR_COLOR);
}

void drawContent(LGFX_Sprite& canvas, uint16_t topLine) {
  canvas.setTextColor(UI_TEXT_COLOR);
  canvas.setTextDatum(lgfx::textdatum_t::top_left);
  const uint16_t last = std::min<uint16_t>(lineCount, topLine + messageViewVisibleLines());
  // Una linea nunca pasa de CONTENT_WIDTH pixeles: con 1 px por caracter como
  // minimo, cabe en este buffer
  char text[CONTENT_WIDTH + 1];
  for (uint16_t i = topLine; i < last; ++i) {
    const bool isTitle = i < titleLineCount;
    if (isTitle) {
      setTitleFont(canvas);
    } else {
      setContentFont(canvas);
    }
    const char* source = isTitle ? title : content;
    const size_t len = std::min<size_t>(lines[i].length, sizeof(text) - 1);
    memcpy(text, source + lines[i].start, len);
    text[len] = '\0';
    canvas.drawString(text, CONTENT_X, CONTENT_TOP + (i - topLine) * lineHeight());
  }
}

void drawScrollbar(LGFX_Sprite& canvas, uint16_t topLine) {
  const uint16_t visible = messageViewVisibleLines();
  if (lineCount <= visible) {
    return;
  }
  const int32_t trackLen = CONTENT_BOTTOM - CONTENT_TOP;
  const ScrollThumb thumb = scrollThumb(lineCount, visible, topLine, trackLen, SCROLLBAR_MIN_LEN);
  canvas.fillRoundRect(SCROLLBAR_X, CONTENT_TOP, SCROLLBAR_WIDTH, trackLen, SCROLLBAR_WIDTH / 2, UI_SEPARATOR_COLOR);
  canvas.fillRoundRect(SCROLLBAR_X, CONTENT_TOP + thumb.offset, SCROLLBAR_WIDTH, thumb.length, SCROLLBAR_WIDTH / 2,
                       UI_DIM_COLOR);
}
}  // namespace

void messageViewOpen(LGFX_Sprite& canvas, const MessageSummary& msg, bool isInTrash) {
  current = msg;
  isCurrentInTrash = isInTrash;
  loadTexts(msg);
  wrapTexts(canvas);
  logMsg(TAG, "abierto %s: %u lineas", msg.id, static_cast<unsigned>(lineCount));
}

const char* messageViewId() {
  return current.id;
}

void messageViewSetFavorite(bool isFavorite) {
  current.isFavorite = isFavorite;
}

uint16_t messageViewLineCount() {
  return lineCount;
}

uint16_t messageViewVisibleLines() {
  return static_cast<uint16_t>((CONTENT_BOTTOM - CONTENT_TOP) / lineHeight());
}

void messageViewDraw(LGFX_Sprite& canvas, uint16_t topLine, bool isOnline) {
  // El autor hace de titulo de la pantalla
  char author[MSG_AUTHOR_SIZE];
  truncateForRow(current.autor, MSG_AUTHOR_MAX_CHARS, author, sizeof(author));
  menuDrawHeader(canvas, author, isOnline, OfflineIconSide::Left);
  uiDrawFavoriteBadge(canvas, current.isFavorite);
  drawSubtitle(canvas);
  drawContent(canvas, topLine);
  drawScrollbar(canvas, topLine);
  const bool canScroll = lineCount > messageViewVisibleLines();
  if (isCurrentInTrash) {
    menuDrawFooter(canvas, canScroll ? FOOTER_TRASH_SCROLL : FOOTER_TRASH_BACK, FOOTER_Y);
  } else {
    menuDrawFooter(canvas, canScroll ? FOOTER_SCROLL : FOOTER_BACK, FOOTER_Y);
  }
}
