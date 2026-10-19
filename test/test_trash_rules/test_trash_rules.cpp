#include <unity.h>

#include "deletion_queue_rules.h"
#include "trash_rules.h"

namespace {
ButtonEvent tap(Button b) {
  return ButtonEvent{b, false, false};
}

ButtonEvent longPress(Button b) {
  return ButtonEvent{b, false, true};
}

MessageSummary msg(const char* id) {
  MessageSummary m;
  strncpy(m.id, id, sizeof(m.id) - 1);
  return m;
}

constexpr uint16_t VISIBLE = 4;
// ~8 KB: fuera de la pila de los tests
DeletionQueueIds queue;

}  // namespace

void setUp() {}
void tearDown() {}

// ----- Rutas --------------------------------------------------------------------

void test_paths_to_trash_move_json_and_mp3_into_papelera() {
  TrashPaths p{};
  TEST_ASSERT_TRUE(trashPathsFor("abc123", TrashDirection::ToTrash, p));
  TEST_ASSERT_EQUAL_STRING("/mensajes/abc123.json", p.fromJson);
  TEST_ASSERT_EQUAL_STRING("/papelera/mensajes/abc123.json", p.toJson);
  TEST_ASSERT_EQUAL_STRING("/audios/abc123.mp3", p.fromAudio);
  TEST_ASSERT_EQUAL_STRING("/papelera/audios/abc123.mp3", p.toAudio);
}

void test_paths_from_trash_are_the_reverse() {
  TrashPaths to{};
  TrashPaths back{};
  trashPathsFor("abc123", TrashDirection::ToTrash, to);
  TEST_ASSERT_TRUE(trashPathsFor("abc123", TrashDirection::FromTrash, back));
  TEST_ASSERT_EQUAL_STRING(to.toJson, back.fromJson);
  TEST_ASSERT_EQUAL_STRING(to.fromJson, back.toJson);
  TEST_ASSERT_EQUAL_STRING(to.toAudio, back.fromAudio);
  TEST_ASSERT_EQUAL_STRING(to.fromAudio, back.toAudio);
}

void test_paths_reject_invalid_ids_and_fit_the_longest_one() {
  TrashPaths p{};
  TEST_ASSERT_FALSE(trashPathsFor("../config", TrashDirection::ToTrash, p));
  TEST_ASSERT_FALSE(trashPathsFor("", TrashDirection::ToTrash, p));
  TEST_ASSERT_FALSE(trashPathsFor(nullptr, TrashDirection::FromTrash, p));
  const char* longest = "0123456789012345678901234567890123456789";  // 40
  TEST_ASSERT_TRUE(trashPathsFor(longest, TrashDirection::ToTrash, p));
  TEST_ASSERT_EQUAL_size_t(strlen("/papelera/mensajes/") + 40 + strlen(".json"), strlen(p.toJson));
}

void test_visible_trash_skips_queued_ids() {
  const MessageSummary items[3] = {msg("a"), msg("b"), msg("c")};
  queueClear(queue);
  queueAdd(queue, "b");
  uint16_t rows[3];
  auto isQueued = [](const char* id) { return queueContains(queue, id); };
  TEST_ASSERT_EQUAL_UINT16(2, trashVisibleIndex(items, 3, isQueued, rows, 3));
  TEST_ASSERT_EQUAL_UINT16(0, rows[0]);
  TEST_ASSERT_EQUAL_UINT16(2, rows[1]);
}

// ----- Navegacion -----------------------------------------------------------------

void test_list_opens_moves_and_back_returns_to_menu() {
  const MessageListModel top{};
  TEST_ASSERT_EQUAL(TrashListAction::Open, trashListOnButton(top, tap(Button::BTN3_OK), 3, VISIBLE).action);
  TEST_ASSERT_EQUAL(TrashListAction::Back, trashListOnButton(top, tap(Button::BTN4_BACK), 3, VISIBLE).action);
  TEST_ASSERT_EQUAL_UINT16(1, trashListOnButton(top, tap(Button::BTN2_DOWN), 3, VISIBLE).model.selected);
}

void test_list_without_rows_does_not_open_and_long_press_waits_for_hold() {
  const MessageListModel top{};
  TEST_ASSERT_EQUAL(TrashListAction::None, trashListOnButton(top, tap(Button::BTN3_OK), 0, VISIBLE).action);
  TEST_ASSERT_EQUAL(TrashListAction::None, trashListOnButton(top, longPress(Button::BTN3_OK), 3, VISIBLE).action);
  TEST_ASSERT_EQUAL(TrashListAction::None, trashListOnButton(top, longPress(Button::BTN4_BACK), 3, VISIBLE).action);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_paths_to_trash_move_json_and_mp3_into_papelera);
  RUN_TEST(test_paths_from_trash_are_the_reverse);
  RUN_TEST(test_paths_reject_invalid_ids_and_fit_the_longest_one);
  RUN_TEST(test_visible_trash_skips_queued_ids);
  RUN_TEST(test_list_opens_moves_and_back_returns_to_menu);
  RUN_TEST(test_list_without_rows_does_not_open_and_long_press_waits_for_hold);
  return UNITY_END();
}
