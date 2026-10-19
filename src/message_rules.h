#pragma once

// Logica pura de la lista y el visor de mensajes, sin dependencias de
// Arduino: se compila tambien en el entorno native para los tests (pio test
// -e native). Leer la SD y dibujar es cosa de message_list y message_view.

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>

#include "button_rules.h"

// ----- Resumen de un mensaje (una fila de la lista) ---------------------------

// Igual que MSG_ID_MAX_LEN de config.h (aqui no se puede incluir config.h)
constexpr size_t MSG_SUMMARY_ID_MAX = 40;
constexpr size_t MSG_AUTHOR_SIZE = 32;
constexpr size_t MSG_TITLE_SIZE = 48;
// "2026-09-22T08:15:00Z" son 20; se deja margen para milisegundos
constexpr size_t MSG_DATE_SIZE = 32;

struct MessageSummary {
  char id[MSG_SUMMARY_ID_MAX + 1] = "";
  char autor[MSG_AUTHOR_SIZE] = "";
  char titulo[MSG_TITLE_SIZE] = "";
  // ISO 8601 UTC tal como llega del backend
  char fecha[MSG_DATE_SIZE] = "";
  bool isAudio = false;
  bool isRead = false;
  // Campo "favorito" del JSON; los mensajes viejos no lo tienen: false
  bool isFavorite = false;
};

// "tipo" del JSON: "audio" o "texto". Cualquier otro valor se trata como texto.
inline bool isAudioType(const char* tipo) {
  return tipo != nullptr && strcmp(tipo, "audio") == 0;
}

// true si `a` va antes que `b` en la lista: mas nuevo primero. Las fechas ISO
// del backend tienen todas el mismo formato, asi que se comparan como texto.
// A igual fecha decide el id, para que el orden no dependa de la SD. Sin
// fecha va al final.
inline bool compareMessages(const MessageSummary& a, const MessageSummary& b) {
  const int byDate = strcmp(a.fecha, b.fecha);
  if (byDate != 0) {
    return byDate > 0;
  }
  return strcmp(a.id, b.id) > 0;
}

inline size_t countUnread(const MessageSummary* items, size_t count) {
  size_t unread = 0;
  for (size_t i = 0; i < count; ++i) {
    if (!items[i].isRead) {
      ++unread;
    }
  }
  return unread;
}

// Copia del resumen marcada como leida; no toca el original
inline MessageSummary withRead(const MessageSummary& msg) {
  MessageSummary next = msg;
  next.isRead = true;
  return next;
}

// Copia del resumen con el favorito cambiado; no toca el original
inline MessageSummary withFavoriteToggled(const MessageSummary& msg) {
  MessageSummary next = msg;
  next.isFavorite = !msg.isFavorite;
  return next;
}

// Filas que se ven en la lista: indices de `items` en el mismo orden, todos o
// solo los favoritos. Devuelve cuantos escribio (como mucho outCapacity).
inline uint16_t buildListIndex(const MessageSummary* items, size_t count, bool isFavoritesOnly, uint16_t* out,
                               size_t outCapacity) {
  uint16_t written = 0;
  for (size_t i = 0; i < count && written < outCapacity; ++i) {
    if (!isFavoritesOnly || items[i].isFavorite) {
      out[written++] = static_cast<uint16_t>(i);
    }
  }
  return written;
}

// Anade `item` a un array de capacidad fija y devuelve la nueva cuenta. Si
// esta lleno se queda con los mas nuevos: sustituye al mas antiguo solo si
// `item` es mas nuevo que el.
inline size_t messageListInsert(MessageSummary* items, size_t count, size_t capacity, const MessageSummary& item) {
  if (count < capacity) {
    items[count] = item;
    return count + 1;
  }
  if (capacity == 0) {
    return 0;
  }
  size_t oldest = 0;
  for (size_t i = 1; i < count; ++i) {
    if (compareMessages(items[oldest], items[i])) {
      oldest = i;
    }
  }
  if (compareMessages(item, items[oldest])) {
    items[oldest] = item;
  }
  return count;
}

// ----- Texto --------------------------------------------------------------------

// Las fuentes de la UI (fonts/fonts.h) cubren Latin-1 (0x20-0xFF) y el canvas
// dibuja un byte por caracter. Convierte UTF-8 a Latin-1: "a" con tilde
// (C3 A1) pasa a ser el byte E1. Lo que no esta en Latin-1 (emojis, euro...)
// y los controles C1 desaparecen; el espacio duro pasa a espacio normal.
// Siempre termina `out` en '\0'. Devuelve la longitud escrita.
inline size_t toDisplayLatin1(const char* in, char* out, size_t outSize) {
  if (outSize == 0) {
    return 0;
  }
  constexpr uint8_t LEAD_C2 = 0xC2;  // U+0080..U+00BF
  constexpr uint8_t LEAD_C3 = 0xC3;  // U+00C0..U+00FF
  constexpr uint8_t CONT_FIRST = 0x80;
  constexpr uint8_t CONT_LAST = 0xBF;
  constexpr uint8_t NBSP = 0xA0;
  size_t written = 0;
  const auto* p = reinterpret_cast<const uint8_t*>(in != nullptr ? in : "");
  while (*p != 0 && written + 1 < outSize) {
    const uint8_t c = *p;
    if (c < 0x80) {
      out[written++] = static_cast<char>(c);
      ++p;
      continue;
    }
    const bool isLatin1 = (c == LEAD_C2 || c == LEAD_C3) && p[1] >= CONT_FIRST && p[1] <= CONT_LAST;
    if (isLatin1) {
      const auto code = static_cast<uint8_t>(c == LEAD_C3 ? p[1] + 0x40 : p[1]);
      if (code == NBSP) {
        out[written++] = ' ';
      } else if (code > NBSP) {
        out[written++] = static_cast<char>(code);
      }
      p += 2;
      continue;
    }
    // Resto de secuencias: se salta el caracter entero (1 byte inicial + los de continuacion)
    ++p;
    while ((*p & 0xC0) == 0x80) {
      ++p;
    }
  }
  out[written] = '\0';
  return written;
}

// Copia `text` en `out` con como mucho maxChars caracteres. Si no cabe, corta
// y termina en "..." (contados dentro de maxChars), sin espacios antes.
inline void truncateForRow(const char* text, size_t maxChars, char* out, size_t outSize) {
  if (outSize == 0) {
    return;
  }
  const char* src = text != nullptr ? text : "";
  const size_t limit = maxChars < outSize - 1 ? maxChars : outSize - 1;
  const size_t len = strlen(src);
  if (len <= limit) {
    memcpy(out, src, len);
    out[len] = '\0';
    return;
  }
  constexpr size_t ELLIPSIS_LEN = 3;
  if (limit <= ELLIPSIS_LEN) {
    memcpy(out, src, limit);
    out[limit] = '\0';
    return;
  }
  size_t keep = limit - ELLIPSIS_LEN;
  while (keep > 0 && src[keep - 1] == ' ') {
    --keep;
  }
  memcpy(out, src, keep);
  memcpy(out + keep, "...", ELLIPSIS_LEN + 1);
}

// Una linea del visor: trozo [start, start + length) del texto
struct TextLine {
  uint16_t start;
  uint16_t length;
};

// Parte `text` en lineas de como mucho maxWidth pixeles. measure(ptr, len)
// devuelve el ancho en pixeles de len caracteres. Corta entre palabras; una
// palabra que no cabe sola se parte por caracteres. Respeta los '\n'. Devuelve
// cuantas lineas escribio (como mucho maxLines).
template <typename MeasureFn>
uint16_t wrapText(const char* text, int32_t maxWidth, MeasureFn measure, TextLine* lines, uint16_t maxLines) {
  const size_t len = strlen(text);
  size_t pos = 0;
  uint16_t count = 0;
  while (pos < len && count < maxLines) {
    size_t lineEnd = pos;
    size_t scan = pos;
    bool isOverflow = false;
    while (scan < len && text[scan] != '\n') {
      size_t wordEnd = scan;
      while (wordEnd < len && text[wordEnd] != ' ' && text[wordEnd] != '\n') {
        ++wordEnd;
      }
      if (measure(text + pos, wordEnd - pos) > maxWidth) {
        isOverflow = true;
        break;
      }
      lineEnd = wordEnd;
      scan = wordEnd;
      while (scan < len && text[scan] == ' ') {
        ++scan;
      }
    }
    if (isOverflow && lineEnd == pos) {
      // Ni la primera palabra cabe: se parte por caracteres (al menos uno)
      size_t n = 1;
      while (pos + n < len && text[pos + n] != ' ' && text[pos + n] != '\n' &&
             measure(text + pos, n + 1) <= maxWidth) {
        ++n;
      }
      lines[count++] = {static_cast<uint16_t>(pos), static_cast<uint16_t>(n)};
      pos += n;
      continue;
    }
    lines[count++] = {static_cast<uint16_t>(pos), static_cast<uint16_t>(lineEnd - pos)};
    pos = (!isOverflow && scan < len) ? scan + 1 : scan;  // se come el '\n'
  }
  return count;
}

// ----- Fechas -------------------------------------------------------------------

struct DateTimeParts {
  int32_t year;
  int32_t month;   // 1-12
  int32_t day;     // 1-31
  int32_t hour;    // 0-23
  int32_t minute;  // 0-59
};

// Dias desde 1970-01-01 (algoritmo de H. Hinnant, calendario gregoriano)
inline int32_t daysFromCivil(int32_t y, int32_t m, int32_t d) {
  y -= m <= 2 ? 1 : 0;
  const int32_t era = (y >= 0 ? y : y - 399) / 400;
  const int32_t yoe = y - era * 400;
  const int32_t doy = (153 * (m > 2 ? m - 3 : m + 9) + 2) / 5 + d - 1;
  const int32_t doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return era * 146097 + doe - 719468;
}

inline void civilFromDays(int32_t z, int32_t& y, int32_t& m, int32_t& d) {
  z += 719468;
  const int32_t era = (z >= 0 ? z : z - 146096) / 146097;
  const int32_t doe = z - era * 146097;
  const int32_t yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
  const int32_t doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
  const int32_t mp = (5 * doy + 2) / 153;
  d = doy - (153 * mp + 2) / 5 + 1;
  m = mp < 10 ? mp + 3 : mp - 9;
  y = yoe + era * 400 + (m <= 2 ? 1 : 0);
}

// Lee "AAAA-MM-DDTHH:MM" del principio; lo que siga (segundos, 'Z') se ignora
inline bool parseIsoUtc(const char* iso, DateTimeParts& out) {
  constexpr size_t MIN_LEN = 16;
  if (iso == nullptr || strnlen(iso, MIN_LEN) < MIN_LEN) {
    return false;
  }
  constexpr uint8_t DIGIT_POS[] = {0, 1, 2, 3, 5, 6, 8, 9, 11, 12, 14, 15};
  for (uint8_t i : DIGIT_POS) {
    if (iso[i] < '0' || iso[i] > '9') {
      return false;
    }
  }
  if (iso[4] != '-' || iso[7] != '-' || iso[10] != 'T' || iso[13] != ':') {
    return false;
  }
  auto num = [iso](size_t at, size_t digits) {
    int32_t v = 0;
    for (size_t i = 0; i < digits; ++i) {
      v = v * 10 + (iso[at + i] - '0');
    }
    return v;
  };
  const DateTimeParts p{num(0, 4), num(5, 2), num(8, 2), num(11, 2), num(14, 2)};
  if (p.month < 1 || p.month > 12 || p.day < 1 || p.day > 31 || p.hour > 23 || p.minute > 59) {
    return false;
  }
  out = p;
  return true;
}

// Minutos que la hora peninsular espanola va por delante de UTC: +60 en
// invierno, +120 en verano (del ultimo domingo de marzo al ultimo de octubre,
// ambos a la 01:00 UTC, regla de la UE).
inline int32_t spainUtcOffsetMinutes(const DateTimeParts& utc) {
  constexpr int32_t MIN_PER_DAY = 24 * 60;
  constexpr int32_t CHANGE_UTC_MIN = 60;
  auto lastSundayOf = [](int32_t year, int32_t month) {
    const int32_t day31 = daysFromCivil(year, month, 31);
    // 1970-01-01 fue jueves: (dias + 4) % 7 da 0 = domingo
    const int32_t weekday = ((day31 + 4) % 7 + 7) % 7;
    return day31 - weekday;
  };
  const int32_t t = daysFromCivil(utc.year, utc.month, utc.day) * MIN_PER_DAY + utc.hour * 60 + utc.minute;
  const int32_t summerStart = lastSundayOf(utc.year, 3) * MIN_PER_DAY + CHANGE_UTC_MIN;
  const int32_t summerEnd = lastSundayOf(utc.year, 10) * MIN_PER_DAY + CHANGE_UTC_MIN;
  return (t >= summerStart && t < summerEnd) ? 120 : 60;
}

inline DateTimeParts addMinutes(const DateTimeParts& p, int32_t minutes) {
  constexpr int32_t MIN_PER_DAY = 24 * 60;
  int32_t total = p.hour * 60 + p.minute + minutes;
  int32_t days = daysFromCivil(p.year, p.month, p.day);
  days += total >= 0 ? total / MIN_PER_DAY : (total - MIN_PER_DAY + 1) / MIN_PER_DAY;
  total = ((total % MIN_PER_DAY) + MIN_PER_DAY) % MIN_PER_DAY;
  DateTimeParts next{0, 0, 0, total / 60, total % 60};
  civilFromDays(days, next.year, next.month, next.day);
  return next;
}

// "22 sep" o, con withTime, "22 sep 10:15", en hora peninsular espanola. Si
// la fecha no es valida deja `out` vacio y devuelve false.
inline bool formatDate(const char* iso, bool withTime, char* out, size_t outSize) {
  static const char* const MONTHS[] = {"ene", "feb", "mar", "abr", "may", "jun",
                                       "jul", "ago", "sep", "oct", "nov", "dic"};
  if (outSize == 0) {
    return false;
  }
  out[0] = '\0';
  DateTimeParts utc{};
  if (!parseIsoUtc(iso, utc)) {
    return false;
  }
  const DateTimeParts local = addMinutes(utc, spainUtcOffsetMinutes(utc));
  const char* month = MONTHS[local.month - 1];
  if (withTime) {
    snprintf(out, outSize, "%d %s %02d:%02d", static_cast<int>(local.day), month, static_cast<int>(local.hour),
             static_cast<int>(local.minute));
  } else {
    snprintf(out, outSize, "%d %s", static_cast<int>(local.day), month);
  }
  return true;
}

// ----- Navegacion -----------------------------------------------------------------

// Seleccion y primera fila visible de la lista
struct MessageListModel {
  uint16_t selected = 0;
  uint16_t first = 0;
};

inline bool operator==(const MessageListModel& a, const MessageListModel& b) {
  return a.selected == b.selected && a.first == b.first;
}

enum class MessageListExit : uint8_t { None, Open, Back };

struct MessageListStep {
  MessageListModel model;
  MessageListExit exit;
};

// Modelo valido para `total` filas con `visible` en pantalla: la seleccion
// dentro de la lista y la ventana moviendose lo justo para que se vea.
inline MessageListModel messageListClamp(const MessageListModel& m, uint16_t total, uint16_t visible) {
  if (total == 0 || visible == 0) {
    return {};
  }
  const uint16_t selected = m.selected < total ? m.selected : static_cast<uint16_t>(total - 1);
  uint16_t first = m.first;
  if (selected < first) {
    first = selected;
  } else if (selected >= first + visible) {
    first = static_cast<uint16_t>(selected - visible + 1);
  }
  const uint16_t maxFirst = total > visible ? static_cast<uint16_t>(total - visible) : 0;
  return {selected, first < maxFirst ? first : maxFirst};
}

// 1/2 mueven (sin dar la vuelta: en una lista larga saltar del primero al
// ultimo despista), 3 abre, 4 vuelve. La pulsacion larga no hace nada.
inline MessageListStep messageListOnButton(const MessageListModel& m, const ButtonEvent& ev, uint16_t total,
                                           uint16_t visible) {
  if (ev.longPress) {
    return {m, MessageListExit::None};
  }
  switch (ev.button) {
    case Button::BTN1_UP: {
      const uint16_t selected = m.selected > 0 ? static_cast<uint16_t>(m.selected - 1) : 0;
      return {messageListClamp({selected, m.first}, total, visible), MessageListExit::None};
    }
    case Button::BTN2_DOWN:
      return {messageListClamp({static_cast<uint16_t>(m.selected + 1), m.first}, total, visible),
              MessageListExit::None};
    case Button::BTN3_OK:
      return {m, total > 0 ? MessageListExit::Open : MessageListExit::None};
    case Button::BTN4_BACK:
      return {m, MessageListExit::Back};
    case Button::NONE:
      break;
  }
  return {m, MessageListExit::None};
}

// Mantener un boton UI_LONG_HOLD_MS en la lista: solo el 4 hace algo, pedir
// borrar el seleccionado (como en el visor). Mantener 3 no hace nada.
inline bool messageListHoldAsksDelete(Button button, uint16_t total) {
  return button == Button::BTN4_BACK && total > 0;
}

struct MessageViewStep {
  uint16_t topLine;
  bool isBack;
  bool isToggleFavorite;
};

// Visor de texto: 1/2 suben/bajan `step` lineas sin pasarse del final, 3
// marca o desmarca favorito, 4 vuelve a la lista. El 3 largo tambien marca
// favorito, igual que en el visor de audio (donde el corto es play/pausa).
inline MessageViewStep messageViewOnButton(uint16_t topLine, const ButtonEvent& ev, uint16_t totalLines,
                                           uint16_t visibleLines, uint16_t step) {
  if (ev.button == Button::BTN3_OK) {
    return {topLine, false, true};
  }
  if (ev.longPress) {
    return {topLine, false, false};
  }
  const uint16_t maxTop = totalLines > visibleLines ? static_cast<uint16_t>(totalLines - visibleLines) : 0;
  switch (ev.button) {
    case Button::BTN1_UP:
      return {topLine > step ? static_cast<uint16_t>(topLine - step) : static_cast<uint16_t>(0), false, false};
    case Button::BTN2_DOWN: {
      const uint32_t next = static_cast<uint32_t>(topLine) + step;
      return {next < maxTop ? static_cast<uint16_t>(next) : maxTop, false, false};
    }
    case Button::BTN4_BACK:
      return {topLine, true, false};
    case Button::BTN3_OK:
    case Button::NONE:
      break;
  }
  return {topLine, false, false};
}

// Posicion y largo del cursor de una barra de scroll de trackLen pixeles que
// muestra `visible` de `total` elementos empezando en `first`.
struct ScrollThumb {
  int32_t offset;
  int32_t length;
};

inline ScrollThumb scrollThumb(uint32_t total, uint32_t visible, uint32_t first, int32_t trackLen, int32_t minLen) {
  if (total <= visible || total == 0) {
    return {0, trackLen};
  }
  int32_t length = static_cast<int32_t>(static_cast<int64_t>(trackLen) * visible / total);
  length = length < minLen ? minLen : length;
  const uint32_t maxFirst = total - visible;
  const uint32_t clampedFirst = first < maxFirst ? first : maxFirst;
  const int32_t offset = static_cast<int32_t>(static_cast<int64_t>(trackLen - length) * clampedFirst / maxFirst);
  return {offset, length};
}

// noLeidos tras abrir un mensaje que no estaba leido; nunca baja de 0
inline uint32_t unreadAfterRead(uint32_t unread) {
  return unread > 0 ? unread - 1 : 0;
}
