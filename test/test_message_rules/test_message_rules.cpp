#include <unity.h>

#include <algorithm>

#include "message_rules.h"

namespace {
MessageSummary msg(const char* id, const char* fecha, bool isRead = false) {
  MessageSummary m;
  strncpy(m.id, id, sizeof(m.id) - 1);
  strncpy(m.fecha, fecha, sizeof(m.fecha) - 1);
  m.isRead = isRead;
  return m;
}

ButtonEvent tap(Button b) {
  return ButtonEvent{b, false, false};
}

ButtonEvent longPress(Button b) {
  return ButtonEvent{b, false, true};
}

// Fuente de ancho fijo: 10 px por caracter
int32_t fixedWidth(const char*, size_t len) {
  return static_cast<int32_t>(len) * 10;
}

constexpr size_t OUT_SIZE = 32;
constexpr uint16_t MAX_LINES = 16;
}  // namespace

void setUp() {}
void tearDown() {}

// ----- Fechas -------------------------------------------------------------------

void test_format_date_short_and_with_time_in_spain_summer_time() {
  char out[OUT_SIZE];
  TEST_ASSERT_TRUE(formatDate("2026-09-22T08:15:00Z", false, out, sizeof(out)));
  TEST_ASSERT_EQUAL_STRING("22 sep", out);
  TEST_ASSERT_TRUE(formatDate("2026-09-22T08:15:00Z", true, out, sizeof(out)));
  TEST_ASSERT_EQUAL_STRING("22 sep 10:15", out);
}

void test_format_date_uses_winter_time_and_changes_day_and_year() {
  char out[OUT_SIZE];
  TEST_ASSERT_TRUE(formatDate("2026-01-05T09:00:00Z", true, out, sizeof(out)));
  TEST_ASSERT_EQUAL_STRING("5 ene 10:00", out);
  // 23:30 UTC del 31 de diciembre ya es 1 de enero en Espana
  TEST_ASSERT_TRUE(formatDate("2026-12-31T23:30:00Z", true, out, sizeof(out)));
  TEST_ASSERT_EQUAL_STRING("1 ene 00:30", out);
}

void test_format_date_changes_offset_on_last_sunday_at_one_utc() {
  char out[OUT_SIZE];
  // 2026: cambio de hora el 29 de marzo y el 25 de octubre
  formatDate("2026-03-29T00:59:00Z", true, out, sizeof(out));
  TEST_ASSERT_EQUAL_STRING("29 mar 01:59", out);
  formatDate("2026-03-29T01:00:00Z", true, out, sizeof(out));
  TEST_ASSERT_EQUAL_STRING("29 mar 03:00", out);
  formatDate("2026-10-25T00:59:00Z", true, out, sizeof(out));
  TEST_ASSERT_EQUAL_STRING("25 oct 02:59", out);
  formatDate("2026-10-25T01:00:00Z", true, out, sizeof(out));
  TEST_ASSERT_EQUAL_STRING("25 oct 02:00", out);
}

void test_format_date_rejects_invalid_input() {
  char out[OUT_SIZE] = "x";
  TEST_ASSERT_FALSE(formatDate("", false, out, sizeof(out)));
  TEST_ASSERT_EQUAL_STRING("", out);
  TEST_ASSERT_FALSE(formatDate("2026-13-01T00:00:00Z", false, out, sizeof(out)));
  TEST_ASSERT_FALSE(formatDate("2026/09/22 08:15", false, out, sizeof(out)));
  TEST_ASSERT_FALSE(formatDate(nullptr, false, out, sizeof(out)));
}

// ----- Orden y lista ---------------------------------------------------------------

void test_compare_messages_sorts_newest_first_and_breaks_ties_by_id() {
  MessageSummary items[] = {msg("a", "2026-09-20T10:00:00Z"), msg("b", ""), msg("c", "2026-09-22T08:00:00Z"),
                            msg("d", "2026-09-20T10:00:00Z")};
  std::sort(std::begin(items), std::end(items), compareMessages);
  TEST_ASSERT_EQUAL_STRING("c", items[0].id);
  TEST_ASSERT_EQUAL_STRING("d", items[1].id);
  TEST_ASSERT_EQUAL_STRING("a", items[2].id);
  TEST_ASSERT_EQUAL_STRING("b", items[3].id);
}

void test_count_unread_and_with_read_does_not_modify_original() {
  const MessageSummary items[] = {msg("a", "", false), msg("b", "", true), msg("c", "", false)};
  TEST_ASSERT_EQUAL_UINT32(2, countUnread(items, 3));
  TEST_ASSERT_EQUAL_UINT32(0, countUnread(items, 0));
  const MessageSummary read = withRead(items[0]);
  TEST_ASSERT_TRUE(read.isRead);
  TEST_ASSERT_FALSE(items[0].isRead);
}

void test_list_insert_keeps_newest_when_full() {
  MessageSummary items[2];
  size_t count = 0;
  count = messageListInsert(items, count, 2, msg("old", "2026-01-01T00:00:00Z"));
  count = messageListInsert(items, count, 2, msg("mid", "2026-05-01T00:00:00Z"));
  count = messageListInsert(items, count, 2, msg("new", "2026-09-01T00:00:00Z"));
  TEST_ASSERT_EQUAL_UINT32(2, count);
  TEST_ASSERT_EQUAL_STRING("new", items[0].id);
  TEST_ASSERT_EQUAL_STRING("mid", items[1].id);
  // Uno mas antiguo que todos no entra
  count = messageListInsert(items, count, 2, msg("older", "2025-01-01T00:00:00Z"));
  TEST_ASSERT_EQUAL_STRING("new", items[0].id);
  TEST_ASSERT_EQUAL_STRING("mid", items[1].id);
}

void test_unread_after_read_never_goes_below_zero() {
  TEST_ASSERT_EQUAL_UINT32(2, unreadAfterRead(3));
  TEST_ASSERT_EQUAL_UINT32(0, unreadAfterRead(0));
}

// ----- Texto -----------------------------------------------------------------------

void test_truncate_for_row_adds_ellipsis_only_when_needed() {
  char out[OUT_SIZE];
  truncateForRow("Buenos dias", 11, out, sizeof(out));
  TEST_ASSERT_EQUAL_STRING("Buenos dias", out);
  truncateForRow("Buenos dias", 10, out, sizeof(out));
  TEST_ASSERT_EQUAL_STRING("Buenos...", out);
  truncateForRow("Hola", 2, out, sizeof(out));
  TEST_ASSERT_EQUAL_STRING("Ho", out);
  // Nunca pasa del buffer
  char tiny[4];
  truncateForRow("abcdefgh", 20, tiny, sizeof(tiny));
  TEST_ASSERT_EQUAL_STRING("abc", tiny);
}

void test_to_display_latin1_keeps_accents_as_single_bytes() {
  char out[OUT_SIZE];
  toDisplayLatin1("Mam\xC3\xA1 cari\xC3\xB1o", out, sizeof(out));
  TEST_ASSERT_EQUAL_STRING("Mam\xE1 cari\xF1o", out);
  toDisplayLatin1("\xC2\xBF" "Qu\xC3\xA9 tal? \xC2\xA1" "Feliz a\xC3\xB1o!", out, sizeof(out));
  TEST_ASSERT_EQUAL_STRING("\xBFQu\xE9 tal? \xA1" "Feliz a\xF1o!", out);
  toDisplayLatin1("\xC3\x91" "AND\xC3\x9C", out, sizeof(out));
  TEST_ASSERT_EQUAL_STRING("\xD1" "AND\xDC", out);
}

void test_to_display_latin1_drops_symbols_outside_latin1() {
  char out[OUT_SIZE];
  // Emoji de 4 bytes y euro (3 bytes): no estan en la fuente, desaparecen
  toDisplayLatin1("hola \xF0\x9F\x98\x80!", out, sizeof(out));
  TEST_ASSERT_EQUAL_STRING("hola !", out);
  toDisplayLatin1("5\xE2\x82\xAC", out, sizeof(out));
  TEST_ASSERT_EQUAL_STRING("5", out);
  // Espacio duro -> espacio normal (para partir lineas); controles C1 fuera
  toDisplayLatin1("a\xC2\xA0" "b\xC2\x85", out, sizeof(out));
  TEST_ASSERT_EQUAL_STRING("a b", out);
}

void test_to_display_latin1_respects_buffer_and_cut_sequences() {
  char out[4];
  TEST_ASSERT_EQUAL_UINT32(3, toDisplayLatin1("abcdef", out, sizeof(out)));
  TEST_ASSERT_EQUAL_STRING("abc", out);
  TEST_ASSERT_EQUAL_UINT32(3, toDisplayLatin1("\xC3\xA1\xC3\xA9\xC3\xAD\xC3\xB3", out, sizeof(out)));
  TEST_ASSERT_EQUAL_STRING("\xE1\xE9\xED", out);
  char out2[OUT_SIZE];
  toDisplayLatin1("ok\xC3", out2, sizeof(out2));
  TEST_ASSERT_EQUAL_STRING("ok", out2);
}

void test_wrap_text_breaks_between_words_and_on_newlines() {
  TextLine lines[MAX_LINES];
  const char* text = "hola que tal\nadios";
  // 80 px = 8 caracteres
  const uint16_t n = wrapText(text, 80, fixedWidth, lines, MAX_LINES);
  TEST_ASSERT_EQUAL_UINT16(3, n);
  TEST_ASSERT_EQUAL_UINT16(0, lines[0].start);
  TEST_ASSERT_EQUAL_UINT16(8, lines[0].length);  // "hola que"
  TEST_ASSERT_EQUAL_UINT16(9, lines[1].start);   // "tal"
  TEST_ASSERT_EQUAL_UINT16(3, lines[1].length);
  TEST_ASSERT_EQUAL_UINT16(13, lines[2].start);  // "adios"
  TEST_ASSERT_EQUAL_UINT16(5, lines[2].length);
}

void test_wrap_text_splits_long_words_keeps_empty_lines_and_caps_count() {
  TextLine lines[MAX_LINES];
  uint16_t n = wrapText("abcdefghij", 40, fixedWidth, lines, MAX_LINES);
  TEST_ASSERT_EQUAL_UINT16(3, n);  // abcd efgh ij
  TEST_ASSERT_EQUAL_UINT16(4, lines[0].length);
  TEST_ASSERT_EQUAL_UINT16(4, lines[1].start);
  TEST_ASSERT_EQUAL_UINT16(2, lines[2].length);
  n = wrapText("a\n\nb", 40, fixedWidth, lines, MAX_LINES);
  TEST_ASSERT_EQUAL_UINT16(3, n);
  TEST_ASSERT_EQUAL_UINT16(0, lines[1].length);
  n = wrapText("a b c d e f", 10, fixedWidth, lines, 2);
  TEST_ASSERT_EQUAL_UINT16(2, n);
  TEST_ASSERT_EQUAL_UINT16(0, wrapText("", 40, fixedWidth, lines, MAX_LINES));
}

// ----- Navegacion ------------------------------------------------------------------

void test_list_moves_without_wrapping_and_scrolls_window() {
  constexpr uint16_t TOTAL = 6;
  constexpr uint16_t VISIBLE = 4;
  MessageListModel m{};
  m = messageListOnButton(m, tap(Button::BTN1_UP), TOTAL, VISIBLE).model;
  TEST_ASSERT_EQUAL_UINT16(0, m.selected);
  for (int i = 0; i < 4; ++i) {
    m = messageListOnButton(m, tap(Button::BTN2_DOWN), TOTAL, VISIBLE).model;
  }
  TEST_ASSERT_EQUAL_UINT16(4, m.selected);
  TEST_ASSERT_EQUAL_UINT16(1, m.first);
  for (int i = 0; i < 5; ++i) {
    m = messageListOnButton(m, tap(Button::BTN2_DOWN), TOTAL, VISIBLE).model;
  }
  TEST_ASSERT_EQUAL_UINT16(5, m.selected);
  TEST_ASSERT_EQUAL_UINT16(2, m.first);
  for (int i = 0; i < 4; ++i) {
    m = messageListOnButton(m, tap(Button::BTN1_UP), TOTAL, VISIBLE).model;
  }
  TEST_ASSERT_EQUAL_UINT16(1, m.selected);
  TEST_ASSERT_EQUAL_UINT16(1, m.first);
}

void test_list_ok_opens_only_with_messages_and_back_exits() {
  const MessageListModel m{};
  TEST_ASSERT_EQUAL(MessageListExit::Open, messageListOnButton(m, tap(Button::BTN3_OK), 3, 4).exit);
  TEST_ASSERT_EQUAL(MessageListExit::None, messageListOnButton(m, tap(Button::BTN3_OK), 0, 4).exit);
  TEST_ASSERT_EQUAL(MessageListExit::Back, messageListOnButton(m, tap(Button::BTN4_BACK), 0, 4).exit);
  TEST_ASSERT_EQUAL(MessageListExit::None, messageListOnButton(m, longPress(Button::BTN3_OK), 3, 4).exit);
}

void test_list_clamp_fixes_selection_after_list_shrinks() {
  const MessageListModel clamped = messageListClamp({9, 7}, 5, 4);
  TEST_ASSERT_EQUAL_UINT16(4, clamped.selected);
  TEST_ASSERT_EQUAL_UINT16(1, clamped.first);
  TEST_ASSERT_TRUE(messageListClamp({3, 2}, 0, 4) == MessageListModel{});
}

void test_view_scrolls_by_step_within_limits_and_back_returns() {
  // 10 lineas, 4 visibles: la primera visible va de 0 a 6
  TEST_ASSERT_EQUAL_UINT16(3, messageViewOnButton(0, tap(Button::BTN2_DOWN), 10, 4, 3).topLine);
  TEST_ASSERT_EQUAL_UINT16(6, messageViewOnButton(5, tap(Button::BTN2_DOWN), 10, 4, 3).topLine);
  TEST_ASSERT_EQUAL_UINT16(0, messageViewOnButton(2, tap(Button::BTN1_UP), 10, 4, 3).topLine);
  TEST_ASSERT_EQUAL_UINT16(0, messageViewOnButton(0, tap(Button::BTN2_DOWN), 3, 4, 3).topLine);
  TEST_ASSERT_TRUE(messageViewOnButton(4, tap(Button::BTN4_BACK), 10, 4, 3).isBack);
  TEST_ASSERT_FALSE(messageViewOnButton(4, tap(Button::BTN3_OK), 10, 4, 3).isBack);
}

void test_scroll_thumb_fills_track_when_all_visible_and_reaches_end() {
  ScrollThumb t = scrollThumb(3, 4, 0, 100, 10);
  TEST_ASSERT_EQUAL_INT32(0, t.offset);
  TEST_ASSERT_EQUAL_INT32(100, t.length);
  t = scrollThumb(20, 4, 16, 100, 10);
  TEST_ASSERT_EQUAL_INT32(20, t.length);
  TEST_ASSERT_EQUAL_INT32(80, t.offset);
  t = scrollThumb(1000, 4, 0, 100, 10);
  TEST_ASSERT_EQUAL_INT32(10, t.length);
}

// ----- Favoritos y tipo -----------------------------------------------------------

void test_toggle_favorite_flips_and_does_not_modify_original() {
  const MessageSummary original = msg("a", "2026-09-22T08:00:00Z");
  const MessageSummary fav = withFavoriteToggled(original);
  TEST_ASSERT_TRUE(fav.isFavorite);
  TEST_ASSERT_FALSE(original.isFavorite);
  TEST_ASSERT_FALSE(withFavoriteToggled(fav).isFavorite);
  TEST_ASSERT_EQUAL_STRING("a", fav.id);
}

void test_list_index_keeps_all_or_only_favorites_in_order() {
  MessageSummary items[4] = {msg("a", "4"), msg("b", "3"), msg("c", "2"), msg("d", "1")};
  items[1].isFavorite = true;
  items[3].isFavorite = true;
  uint16_t index[4];
  TEST_ASSERT_EQUAL_UINT16(4, buildListIndex(items, 4, false, index, 4));
  TEST_ASSERT_EQUAL_UINT16(2, index[2]);
  TEST_ASSERT_EQUAL_UINT16(2, buildListIndex(items, 4, true, index, 4));
  TEST_ASSERT_EQUAL_UINT16(1, index[0]);
  TEST_ASSERT_EQUAL_UINT16(3, index[1]);
}

void test_list_index_is_empty_without_favorites_and_respects_capacity() {
  const MessageSummary items[3] = {msg("a", "3"), msg("b", "2"), msg("c", "1")};
  uint16_t index[2];
  TEST_ASSERT_EQUAL_UINT16(0, buildListIndex(items, 3, true, index, 2));
  TEST_ASSERT_EQUAL_UINT16(2, buildListIndex(items, 3, false, index, 2));
}

void test_audio_type_detection() {
  TEST_ASSERT_TRUE(isAudioType("audio"));
  TEST_ASSERT_FALSE(isAudioType("texto"));
  TEST_ASSERT_FALSE(isAudioType("Audio"));
  TEST_ASSERT_FALSE(isAudioType(""));
  TEST_ASSERT_FALSE(isAudioType(nullptr));
}

void test_view_ok_short_or_long_toggles_favorite_without_moving() {
  const MessageViewStep shortOk = messageViewOnButton(4, tap(Button::BTN3_OK), 10, 4, 3);
  TEST_ASSERT_TRUE(shortOk.isToggleFavorite);
  TEST_ASSERT_FALSE(shortOk.isBack);
  TEST_ASSERT_EQUAL_UINT16(4, shortOk.topLine);
  TEST_ASSERT_TRUE(messageViewOnButton(4, longPress(Button::BTN3_OK), 10, 4, 3).isToggleFavorite);
  TEST_ASSERT_FALSE(messageViewOnButton(4, tap(Button::BTN2_DOWN), 10, 4, 3).isToggleFavorite);
  TEST_ASSERT_FALSE(messageViewOnButton(4, longPress(Button::BTN4_BACK), 10, 4, 3).isBack);
}

void test_list_hold_back_asks_delete_and_hold_ok_does_nothing() {
  TEST_ASSERT_TRUE(messageListHoldAsksDelete(Button::BTN4_BACK, 3));
  TEST_ASSERT_FALSE(messageListHoldAsksDelete(Button::BTN4_BACK, 0));
  TEST_ASSERT_FALSE(messageListHoldAsksDelete(Button::BTN3_OK, 3));
  // La pulsacion larga normal (500 ms) de 3 tampoco abre ni borra
  const MessageListStep held = messageListOnButton(MessageListModel{}, longPress(Button::BTN3_OK), 3, 4);
  TEST_ASSERT_EQUAL(MessageListExit::None, held.exit);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_format_date_short_and_with_time_in_spain_summer_time);
  RUN_TEST(test_format_date_uses_winter_time_and_changes_day_and_year);
  RUN_TEST(test_format_date_changes_offset_on_last_sunday_at_one_utc);
  RUN_TEST(test_format_date_rejects_invalid_input);
  RUN_TEST(test_compare_messages_sorts_newest_first_and_breaks_ties_by_id);
  RUN_TEST(test_count_unread_and_with_read_does_not_modify_original);
  RUN_TEST(test_list_insert_keeps_newest_when_full);
  RUN_TEST(test_unread_after_read_never_goes_below_zero);
  RUN_TEST(test_truncate_for_row_adds_ellipsis_only_when_needed);
  RUN_TEST(test_to_display_latin1_keeps_accents_as_single_bytes);
  RUN_TEST(test_to_display_latin1_drops_symbols_outside_latin1);
  RUN_TEST(test_to_display_latin1_respects_buffer_and_cut_sequences);
  RUN_TEST(test_wrap_text_breaks_between_words_and_on_newlines);
  RUN_TEST(test_wrap_text_splits_long_words_keeps_empty_lines_and_caps_count);
  RUN_TEST(test_list_moves_without_wrapping_and_scrolls_window);
  RUN_TEST(test_list_ok_opens_only_with_messages_and_back_exits);
  RUN_TEST(test_list_clamp_fixes_selection_after_list_shrinks);
  RUN_TEST(test_view_scrolls_by_step_within_limits_and_back_returns);
  RUN_TEST(test_scroll_thumb_fills_track_when_all_visible_and_reaches_end);
  RUN_TEST(test_toggle_favorite_flips_and_does_not_modify_original);
  RUN_TEST(test_list_index_keeps_all_or_only_favorites_in_order);
  RUN_TEST(test_list_index_is_empty_without_favorites_and_respects_capacity);
  RUN_TEST(test_audio_type_detection);
  RUN_TEST(test_view_ok_short_or_long_toggles_favorite_without_moving);
  RUN_TEST(test_list_hold_back_asks_delete_and_hold_ok_does_nothing);
  return UNITY_END();
}
