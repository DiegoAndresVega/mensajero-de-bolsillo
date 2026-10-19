#include <unity.h>

#include "photo_rules.h"

namespace {
ButtonEvent tap(Button b) {
  return ButtonEvent{b, false, false};
}

constexpr size_t NAME_MAX_LEN = 32;
}  // namespace

void setUp() {}
void tearDown() {}

// ----- Ficheros -------------------------------------------------------------------

void test_photo_file_accepts_jpg_and_jpeg_in_any_case() {
  TEST_ASSERT_TRUE(isPhotoFileName("01.jpg", NAME_MAX_LEN));
  TEST_ASSERT_TRUE(isPhotoFileName("playa.JPG", NAME_MAX_LEN));
  TEST_ASSERT_TRUE(isPhotoFileName("foto.jpeg", NAME_MAX_LEN));
  TEST_ASSERT_TRUE(isPhotoFileName("Foto.JpEg", NAME_MAX_LEN));
}

void test_photo_file_rejects_other_formats_and_hidden_files() {
  TEST_ASSERT_FALSE(isPhotoFileName("foto.png", NAME_MAX_LEN));
  TEST_ASSERT_FALSE(isPhotoFileName("foto.heic", NAME_MAX_LEN));
  TEST_ASSERT_FALSE(isPhotoFileName("jpg", NAME_MAX_LEN));
  TEST_ASSERT_FALSE(isPhotoFileName(".jpg", NAME_MAX_LEN));
  // Ficheros "._" que deja el Finder de macOS al copiar
  TEST_ASSERT_FALSE(isPhotoFileName("._01.jpg", NAME_MAX_LEN));
  TEST_ASSERT_FALSE(isPhotoFileName("01.jpg.tmp", NAME_MAX_LEN));
  TEST_ASSERT_FALSE(isPhotoFileName("", NAME_MAX_LEN));
}

void test_photo_file_rejects_names_longer_than_max() {
  TEST_ASSERT_TRUE(isPhotoFileName("123456.jpg", 10));
  TEST_ASSERT_FALSE(isPhotoFileName("1234567.jpg", 10));
}

// ----- Botones ------------------------------------------------------------------

void test_photos_next_and_previous_wrap_around() {
  TEST_ASSERT_EQUAL_UINT16(1, photosOnButton(0, tap(Button::BTN2_DOWN), 3).index);
  TEST_ASSERT_EQUAL_UINT16(0, photosOnButton(2, tap(Button::BTN2_DOWN), 3).index);
  TEST_ASSERT_EQUAL_UINT16(2, photosOnButton(0, tap(Button::BTN1_UP), 3).index);
  TEST_ASSERT_EQUAL_UINT16(0, photosOnButton(1, tap(Button::BTN1_UP), 3).index);
}

void test_photos_back_returns_and_ok_does_nothing() {
  const PhotoStep back = photosOnButton(1, tap(Button::BTN4_BACK), 3);
  TEST_ASSERT_TRUE(back.isBack);
  TEST_ASSERT_EQUAL_UINT16(1, back.index);
  const PhotoStep ok = photosOnButton(1, tap(Button::BTN3_OK), 3);
  TEST_ASSERT_FALSE(ok.isBack);
  TEST_ASSERT_EQUAL_UINT16(1, ok.index);
}

void test_photos_long_press_does_nothing() {
  const PhotoStep step = photosOnButton(1, ButtonEvent{Button::BTN2_DOWN, false, true}, 3);
  TEST_ASSERT_EQUAL_UINT16(1, step.index);
  TEST_ASSERT_FALSE(photosOnButton(1, ButtonEvent{Button::BTN4_BACK, false, true}, 3).isBack);
}

void test_photos_without_photos_only_back_works() {
  TEST_ASSERT_EQUAL_UINT16(0, photosOnButton(0, tap(Button::BTN2_DOWN), 0).index);
  TEST_ASSERT_EQUAL_UINT16(0, photosOnButton(0, tap(Button::BTN1_UP), 0).index);
  TEST_ASSERT_TRUE(photosOnButton(0, tap(Button::BTN4_BACK), 0).isBack);
}

void test_photos_index_out_of_range_is_clamped() {
  // La SD tiene menos fotos que antes (no deberia pasar sin reiniciar)
  TEST_ASSERT_EQUAL_UINT16(0, photosClampIndex(5, 3));
  TEST_ASSERT_EQUAL_UINT16(2, photosClampIndex(2, 3));
  TEST_ASSERT_EQUAL_UINT16(0, photosClampIndex(4, 0));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_photo_file_accepts_jpg_and_jpeg_in_any_case);
  RUN_TEST(test_photo_file_rejects_other_formats_and_hidden_files);
  RUN_TEST(test_photo_file_rejects_names_longer_than_max);
  RUN_TEST(test_photos_next_and_previous_wrap_around);
  RUN_TEST(test_photos_back_returns_and_ok_does_nothing);
  RUN_TEST(test_photos_long_press_does_nothing);
  RUN_TEST(test_photos_without_photos_only_back_works);
  RUN_TEST(test_photos_index_out_of_range_is_clamped);
  return UNITY_END();
}
