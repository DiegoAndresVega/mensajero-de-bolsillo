#include "message_list.h"

#include <esp_heap_caps.h>

#include <algorithm>

#include "config.h"
#include "fonts/fonts.h"
#include "log.h"
#include "menu.h"
#include "notifications.h"
#include "state.h"
#include "storage.h"
#include "tasks.h"
#include "ui_icons.h"
#include "ui_style.h"
#include "utils.h"

static_assert(MSG_SUMMARY_ID_MAX == MSG_ID_MAX_LEN, "el id del resumen debe caber igual que en state");

namespace {
constexpr const char* TAG = "MSG";
constexpr const char* TITLE = "Mensajes";
constexpr const char* FAVORITES_TITLE = "Favoritos";
constexpr const char* LOADING_TEXT = "Cargando...";
constexpr const char* EMPTY_TEXT = "No hay mensajes";
constexpr const char* EMPTY_FAVORITES_TEXT = "No hay favoritos";
constexpr const char* EMPTY_FOOTER = "";
constexpr size_t POSITION_TEXT_MAX = 16;
constexpr size_t DATE_TEXT_MAX = 16;

// Casilla del borrado multiple, en lugar del punto de no leido
constexpr int32_t CHECKBOX_SIZE = 14;
constexpr int32_t CHECKBOX_X = 5;
constexpr float CHECK_LINE_WIDTH = 2.0F;

// Seleccion: color de acento al 24 % sobre el fondo
constexpr uint32_t HIGHLIGHT_ALPHA_PCT = 24;
constexpr uint32_t HIGHLIGHT_COLOR = uiBlendRgb(UI_ACCENT_PINK, UI_BG_COLOR, HIGHLIGHT_ALPHA_PCT);

// Layout de 280x240: cabecera (0-40), filas desde ROWS_TOP y pie abajo
constexpr int32_t ROWS_TOP = UI_SEPARATOR_Y + 2;
constexpr int32_t ROWS_HEIGHT = MSG_ROWS_VISIBLE * MSG_ROW_HEIGHT;
constexpr int32_t FOOTER_Y = 229;
constexpr int32_t ROW_PADDING = 2;
constexpr int32_t ROW_RADIUS = 8;
constexpr int32_t ROW_LEFT = 4;
// Barra de scroll a la derecha; las filas acaban antes
constexpr int32_t SCROLLBAR_WIDTH = 4;
constexpr int32_t SCROLLBAR_X = LCD_WIDTH - SCROLLBAR_WIDTH - 4;
constexpr int32_t SCROLLBAR_MIN_LEN = 12;
constexpr int32_t ROW_RIGHT = SCROLLBAR_X - 4;
// Dentro de cada fila: punto de no leido, linea 1 (autor + fecha) y linea 2
// (icono de tipo + titulo)
constexpr int32_t UNREAD_DOT_X = 12;
constexpr int32_t UNREAD_DOT_RADIUS = 4;
constexpr int32_t TEXT_X = 24;
constexpr int32_t LINE1_Y = MSG_ROW_HEIGHT * 3 / 10;
constexpr int32_t LINE2_Y = MSG_ROW_HEIGHT * 3 / 4;
constexpr int32_t TYPE_ICON_W = 16;
constexpr int32_t TYPE_ICON_H = 11;
constexpr int32_t TITLE_X = TEXT_X + TYPE_ICON_W + 6;
constexpr int32_t DATE_RIGHT_X = ROW_RIGHT - 6;
// Corazon de favorito en la linea 2, debajo de la fecha y alineado a su
// derecha (junto al titulo podia confundirse con un emoji del mensaje)
constexpr int32_t ROW_HEART_X = DATE_RIGHT_X - MSG_ROW_HEART_SIZE / 2;
// Con corazon, el titulo se corta antes para no pisarlo
constexpr size_t TITLE_MAX_CHARS_WITH_HEART = MSG_TITLE_MAX_CHARS - 2;

// "No hay mensajes": un ojo triste mirando abajo
constexpr int32_t SAD_EYE_X = LCD_WIDTH / 2;
constexpr int32_t SAD_EYE_Y = 112;
constexpr int32_t SAD_EYE_RX = 30;
constexpr int32_t SAD_EYE_RY = 38;
constexpr int32_t SAD_PUPIL_RADIUS = 12;
constexpr int32_t EMPTY_TEXT_Y = 182;
// "No hay favoritos": un corazon grande en contorno
constexpr int32_t EMPTY_HEART_Y = 112;
constexpr int32_t EMPTY_HEART_SIZE = 80;

// Cache en PSRAM; se reserva en la primera carga y no se libera
MessageSummary* items = nullptr;
uint16_t itemCount = 0;
// Filas que se ven: indices de `items`, todos o solo favoritos
uint16_t rows[MSG_LIST_MAX];
uint16_t rowCount = 0;
bool isFavoritesOnly = false;
bool isLoaded = false;
uint32_t revision = 0;
// ultimoId de state en la ultima carga
char loadedLastId[MSG_ID_MAX_LEN + 1] = "";
}  // namespace

MessageSummary messageSummaryFromJson(JsonObjectConst msg) {
  MessageSummary s;
  strlcpy(s.id, msg["id"] | "", sizeof(s.id));
  toDisplayLatin1(msg["autor"] | "", s.autor, sizeof(s.autor));
  toDisplayLatin1(msg["titulo"] | "", s.titulo, sizeof(s.titulo));
  strlcpy(s.fecha, msg["fecha"] | "", sizeof(s.fecha));
  s.isAudio = isAudioType(msg["tipo"] | "");
  s.isRead = msg["leido"] | false;
  s.isFavorite = msg["favorito"] | false;
  return s;
}

namespace {
void rebuildRows() {
  rowCount = items != nullptr ? buildListIndex(items, itemCount, isFavoritesOnly, rows, MSG_LIST_MAX) : 0;
  ++revision;
}

bool ensureBuffer() {
  if (items != nullptr) {
    return true;
  }
  items = static_cast<MessageSummary*>(heap_caps_malloc(MSG_LIST_MAX * sizeof(MessageSummary), MALLOC_CAP_SPIRAM));
  if (items == nullptr) {
    logMsg(TAG, "sin PSRAM para la lista (%u bytes)", static_cast<unsigned>(MSG_LIST_MAX * sizeof(MessageSummary)));
    return false;
  }
  return true;
}

// ----- Dibujo -------------------------------------------------------------------

void drawTypeIcon(LGFX_Sprite& canvas, const MessageSummary& msg, int32_t cy, uint32_t color, uint32_t bg) {
  const int32_t y = cy - TYPE_ICON_H / 2;
  if (msg.isAudio) {
    uiDrawSpeaker(canvas, TEXT_X, y, TYPE_ICON_W, TYPE_ICON_H, color);
  } else {
    notificationsDrawIcon(canvas, TEXT_X, y, TYPE_ICON_W, TYPE_ICON_H, color, bg, false);
  }
}

// Casilla con borde; marcada, rosa con una V blanca
void drawCheckbox(LGFX_Sprite& canvas, int32_t cy, bool isChecked, uint32_t color) {
  const int32_t y = cy - CHECKBOX_SIZE / 2;
  if (!isChecked) {
    canvas.drawRoundRect(CHECKBOX_X, y, CHECKBOX_SIZE, CHECKBOX_SIZE, 3, color);
    return;
  }
  canvas.fillRoundRect(CHECKBOX_X, y, CHECKBOX_SIZE, CHECKBOX_SIZE, 3, UI_ACCENT_PINK);
  const int32_t x = CHECKBOX_X;
  canvas.drawWideLine(x + 3, cy, x + 6, y + CHECKBOX_SIZE - 4, CHECK_LINE_WIDTH, UI_TEXT_COLOR);
  canvas.drawWideLine(x + 6, y + CHECKBOX_SIZE - 4, x + CHECKBOX_SIZE - 3, y + 3, CHECK_LINE_WIDTH, UI_TEXT_COLOR);
}

// checked: nullptr en la lista normal (punto de no leido); si no, la casilla
void drawRow(LGFX_Sprite& canvas, const MessageSummary& msg, int32_t rowY, bool isSelected, const bool* checked) {
  const uint32_t bg = isSelected ? HIGHLIGHT_COLOR : UI_BG_COLOR;
  const uint32_t secondary = isSelected ? UI_TEXT_COLOR : UI_DIM_COLOR;
  if (isSelected) {
    canvas.fillRoundRect(ROW_LEFT, rowY + ROW_PADDING, ROW_RIGHT - ROW_LEFT, MSG_ROW_HEIGHT - 2 * ROW_PADDING,
                         ROW_RADIUS, bg);
  }
  if (checked != nullptr) {
    drawCheckbox(canvas, rowY + MSG_ROW_HEIGHT / 2, *checked, secondary);
  } else if (!msg.isRead) {
    canvas.fillCircle(UNREAD_DOT_X, rowY + MSG_ROW_HEIGHT / 2, UNREAD_DOT_RADIUS, NOTIFICATION_BADGE_COLOR);
  }
  char text[MSG_TITLE_SIZE];
  canvas.setTextSize(MSG_ROW_FONT_SIZE);
  // Linea 1: autor a la izquierda, fecha a la derecha
  canvas.setFont(&FreeSansBold9pt8b);
  canvas.setTextColor(UI_TEXT_COLOR);
  canvas.setTextDatum(lgfx::textdatum_t::middle_left);
  truncateForRow(msg.autor, MSG_AUTHOR_MAX_CHARS, text, sizeof(text));
  canvas.drawString(text, TEXT_X, rowY + LINE1_Y);
  char date[DATE_TEXT_MAX];
  formatDate(msg.fecha, false, date, sizeof(date));
  canvas.setFont(&FreeSans9pt8b);
  canvas.setTextColor(secondary);
  canvas.setTextDatum(lgfx::textdatum_t::middle_right);
  canvas.drawString(date, DATE_RIGHT_X, rowY + LINE1_Y);
  // Linea 2: icono de tipo, titulo y, a la derecha, el corazon de favorito
  drawTypeIcon(canvas, msg, rowY + LINE2_Y, secondary, bg);
  canvas.setTextDatum(lgfx::textdatum_t::middle_left);
  truncateForRow(msg.titulo, msg.isFavorite ? TITLE_MAX_CHARS_WITH_HEART : MSG_TITLE_MAX_CHARS, text, sizeof(text));
  canvas.drawString(text, TITLE_X, rowY + LINE2_Y);
  if (msg.isFavorite) {
    uiDrawHeart(canvas, ROW_HEART_X, rowY + LINE2_Y, MSG_ROW_HEART_SIZE, UI_ACCENT_PINK);
  }
}

void drawScrollbar(LGFX_Sprite& canvas, const MessageListModel& model, uint16_t count) {
  if (count <= MSG_ROWS_VISIBLE) {
    return;
  }
  const ScrollThumb thumb = scrollThumb(count, MSG_ROWS_VISIBLE, model.first, ROWS_HEIGHT, SCROLLBAR_MIN_LEN);
  canvas.fillRoundRect(SCROLLBAR_X, ROWS_TOP, SCROLLBAR_WIDTH, ROWS_HEIGHT, SCROLLBAR_WIDTH / 2, UI_SEPARATOR_COLOR);
  canvas.fillRoundRect(SCROLLBAR_X, ROWS_TOP + thumb.offset, SCROLLBAR_WIDTH, thumb.length, SCROLLBAR_WIDTH / 2,
                       UI_DIM_COLOR);
}

void drawCenteredText(LGFX_Sprite& canvas, const char* text, int32_t y) {
  canvas.setTextSize(1);
  canvas.setTextDatum(lgfx::textdatum_t::middle_center);
  canvas.setFont(&FreeSans9pt8b);
  canvas.setTextColor(UI_DIM_COLOR);
  canvas.drawString(text, LCD_WIDTH / 2, y);
}

// Ojo triste: ovalo blanco, pupila mirando abajo y el parpado cayendo hacia fuera
void drawEmpty(LGFX_Sprite& canvas, const char* text) {
  canvas.fillEllipse(SAD_EYE_X, SAD_EYE_Y, SAD_EYE_RX, SAD_EYE_RY, UI_TEXT_COLOR);
  canvas.fillCircle(SAD_EYE_X, SAD_EYE_Y + SAD_EYE_RY / 3, SAD_PUPIL_RADIUS, UI_BG_COLOR);
  const int32_t top = SAD_EYE_Y - SAD_EYE_RY - 2;
  const int32_t left = SAD_EYE_X - SAD_EYE_RX - 2;
  const int32_t right = SAD_EYE_X + SAD_EYE_RX + 2;
  canvas.fillTriangle(left, top, right, top, left, SAD_EYE_Y - SAD_EYE_RY / 5, UI_BG_COLOR);
  drawCenteredText(canvas, text, EMPTY_TEXT_Y);
}

void drawEmptyFavorites(LGFX_Sprite& canvas, const char* text) {
  uiDrawHeartOutline(canvas, LCD_WIDTH / 2, EMPTY_HEART_Y, EMPTY_HEART_SIZE, UI_ACCENT_PINK, UI_BG_COLOR);
  drawCenteredText(canvas, text, EMPTY_TEXT_Y);
}

const char* listTitle() {
  return isFavoritesOnly ? FAVORITES_TITLE : TITLE;
}

MessageSummary cachedRowAt(uint16_t i) {
  return messageListAt(i);
}
}  // namespace

bool messageListNeedsLoad() {
  return !isLoaded || strcmp(loadedLastId, stateSnapshot().ultimoId) != 0;
}

void messageListLoad() {
  if (!ensureBuffer()) {
    return;
  }
  const uint32_t startMs = millis();
  // ultimoId antes de leer: si llega un mensaje durante la carga, la proxima vez se recarga
  const SystemState state = stateSnapshot();
  size_t count = 0;
  storageForEachMessage([&count](JsonObjectConst msg) {
    const MessageSummary summary = messageSummaryFromJson(msg);
    if (!isValidMessageId(summary.id, MSG_ID_MAX_LEN)) {
      return;
    }
    count = messageListInsert(items, count, MSG_LIST_MAX, summary);
  });
  std::sort(items, items + count, compareMessages);
  itemCount = static_cast<uint16_t>(count);
  isLoaded = storageIsReady();
  strlcpy(loadedLastId, state.ultimoId, sizeof(loadedLastId));
  rebuildRows();
  // "cargados" y no "leidos": leido es lo que ya se ha abierto en pantalla
  logMsg(TAG, "%u mensajes cargados, %u sin leer, en %lu ms | pila libre ui=%u", static_cast<unsigned>(itemCount),
         static_cast<unsigned>(countUnread(items, itemCount)), static_cast<unsigned long>(millis() - startMs),
         static_cast<unsigned>(uxTaskGetStackHighWaterMark(nullptr)));
}

void messageListSetFavoritesOnly(bool favoritesOnly) {
  if (favoritesOnly == isFavoritesOnly) {
    return;
  }
  isFavoritesOnly = favoritesOnly;
  rebuildRows();
}

uint16_t messageListCount() {
  return rowCount;
}

MessageSummary messageListAt(uint16_t i) {
  return i < rowCount ? items[rows[i]] : MessageSummary{};
}

void messageListMarkRead(uint16_t i) {
  if (i >= rowCount || items[rows[i]].isRead) {
    return;
  }
  MessageSummary& item = items[rows[i]];
  item = withRead(item);
  ++revision;
  tasksRequestMarkRead(item.id);
}

bool messageListToggleFavorite(const char* id) {
  for (uint16_t i = 0; i < itemCount; ++i) {
    if (strcmp(items[i].id, id) != 0) {
      continue;
    }
    items[i] = withFavoriteToggled(items[i]);
    tasksRequestSetFavorite(id, items[i].isFavorite);
    rebuildRows();
    return items[i].isFavorite;
  }
  return false;
}

uint32_t messageListRevision() {
  return revision;
}

void messageListDrawLoading(LGFX_Sprite& canvas, bool isOnline) {
  menuDrawHeader(canvas, listTitle(), isOnline);
  drawCenteredText(canvas, LOADING_TEXT, LCD_HEIGHT / 2);
}

void messageListDraw(LGFX_Sprite& canvas, const MessageListModel& model, bool isOnline) {
  const MessageRows rowsView{listTitle(),
                             rowCount,
                             cachedRowAt,
                             nullptr,
                             isFavoritesOnly ? EMPTY_FAVORITES_TEXT : EMPTY_TEXT,
                             isFavoritesOnly ? ListEmptyIcon::Heart : ListEmptyIcon::SadEye};
  messageListDrawRows(canvas, rowsView, model, isOnline);
}

void messageListDrawRows(LGFX_Sprite& canvas, const MessageRows& source, const MessageListModel& model,
                         bool isOnline) {
  menuDrawHeader(canvas, source.title, isOnline);
  if (source.count == 0) {
    if (source.emptyIcon == ListEmptyIcon::Heart) {
      drawEmptyFavorites(canvas, source.emptyText);
    } else {
      drawEmpty(canvas, source.emptyText);
    }
    menuDrawFooter(canvas, EMPTY_FOOTER, FOOTER_Y);
    return;
  }
  const uint16_t last = std::min<uint16_t>(source.count, model.first + MSG_ROWS_VISIBLE);
  for (uint16_t i = model.first; i < last; ++i) {
    const bool* checked = source.checked != nullptr ? &source.checked[i] : nullptr;
    drawRow(canvas, source.at(i), ROWS_TOP + (i - model.first) * MSG_ROW_HEIGHT, i == model.selected, checked);
  }
  drawScrollbar(canvas, model, source.count);
  char position[POSITION_TEXT_MAX];
  snprintf(position, sizeof(position), "%u/%u", static_cast<unsigned>(model.selected + 1),
           static_cast<unsigned>(source.count));
  menuDrawFooter(canvas, position, FOOTER_Y);
}

void messageListRemove(const char* id) {
  for (uint16_t i = 0; i < itemCount; ++i) {
    if (strcmp(items[i].id, id) != 0) {
      continue;
    }
    for (uint16_t j = i + 1; j < itemCount; ++j) {
      items[j - 1] = items[j];
    }
    --itemCount;
    rebuildRows();
    return;
  }
}

void messageListInvalidate() {
  isLoaded = false;
}
