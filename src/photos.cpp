#include "photos.h"

#include <esp_heap_caps.h>

#include <cstdlib>
#include <cstring>

#include "config.h"
#include "fonts/fonts.h"
#include "log.h"
#include "menu.h"
#include "photo_rules.h"
#include "storage.h"
#include "ui_style.h"

namespace {
constexpr const char* TAG = "FOTO";
constexpr const char* TITLE = "Fotos";
constexpr const char* EMPTY_TEXT = "No hay fotos";
constexpr const char* TOO_BIG_TEXT = "Foto demasiado grande";
constexpr const char* BAD_PHOTO_TEXT = "No se puede abrir la foto";
// Bandas de las fotos que no llenan la pantalla
constexpr uint32_t PHOTO_BG_COLOR = 0x000000U;
constexpr size_t PATH_MAX_LEN = 48;

char names[PHOTOS_MAX][PHOTO_NAME_MAX + 1];
uint16_t count = 0;
bool isLoaded = false;
// JPEG leido de la SD, en PSRAM; se reserva la primera vez y no se libera
uint8_t* jpegBuffer = nullptr;

int compareNames(const void* a, const void* b) {
  return strcmp(static_cast<const char*>(a), static_cast<const char*>(b));
}

void drawNotice(LGFX_Sprite& canvas, const char* text, bool isOnline) {
  menuDrawHeader(canvas, TITLE, isOnline);
  canvas.setFont(&FreeSans9pt8b);
  canvas.setTextSize(1);
  canvas.setTextColor(UI_TEXT_COLOR);
  canvas.setTextDatum(lgfx::textdatum_t::middle_center);
  canvas.drawString(text, LCD_WIDTH / 2, (UI_SEPARATOR_Y + LCD_HEIGHT) / 2);
}

bool ensureBuffer() {
  if (jpegBuffer != nullptr) {
    return true;
  }
  jpegBuffer = static_cast<uint8_t*>(heap_caps_malloc(PHOTO_MAX_BYTES, MALLOC_CAP_SPIRAM));
  if (jpegBuffer == nullptr) {
    logMsg(TAG, "sin PSRAM para las fotos (%u bytes)", static_cast<unsigned>(PHOTO_MAX_BYTES));
    return false;
  }
  return true;
}
}  // namespace

void photosLoadOnce() {
  if (isLoaded) {
    return;
  }
  isLoaded = true;
  bool isTruncated = false;
  storageForEachFileName(DIR_PHOTOS, [&isTruncated](const char* name) {
    if (!isPhotoFileName(name, PHOTO_NAME_MAX)) {
      return;
    }
    if (count >= PHOTOS_MAX) {
      isTruncated = true;
      return;
    }
    strlcpy(names[count], name, sizeof(names[count]));
    ++count;
  });
  qsort(names, count, sizeof(names[0]), compareNames);
  logMsg(TAG, "%u fotos en %s", static_cast<unsigned>(count), DIR_PHOTOS);
  if (isTruncated) {
    logMsg(TAG, "hay mas de %u fotos: el resto no se muestra", static_cast<unsigned>(PHOTOS_MAX));
  }
}

uint16_t photosCount() {
  return count;
}

void photosDraw(LGFX_Sprite& canvas, uint16_t index, bool isOnline) {
  if (count == 0) {
    drawNotice(canvas, EMPTY_TEXT, isOnline);
    return;
  }
  if (!ensureBuffer()) {
    drawNotice(canvas, BAD_PHOTO_TEXT, isOnline);
    return;
  }
  const uint16_t i = photosClampIndex(index, count);
  char path[PATH_MAX_LEN];
  snprintf(path, sizeof(path), "%s/%s", DIR_PHOTOS, names[i]);
  size_t size = 0;
  const FileReadResult read = storageReadFile(path, jpegBuffer, PHOTO_MAX_BYTES, size);
  if (read == FileReadResult::TooBig) {
    logMsg(TAG, "%s pasa de %u bytes", names[i], static_cast<unsigned>(PHOTO_MAX_BYTES));
    drawNotice(canvas, TOO_BIG_TEXT, isOnline);
    return;
  }
  if (read != FileReadResult::Ok) {
    logMsg(TAG, "no se pudo leer %s", names[i]);
    drawNotice(canvas, BAD_PHOTO_TEXT, isOnline);
    return;
  }
  canvas.fillScreen(PHOTO_BG_COLOR);
  // Escala 0 = encajar en la pantalla sin deformar. Una foto preparada ya
  // mide 280x240 y sale tal cual; otra mas pequena o mas grande, centrada.
  const bool isDrawn = canvas.drawJpg(jpegBuffer, size, 0, 0, LCD_WIDTH, LCD_HEIGHT, 0, 0, 0.0F, 0.0F,
                                      lgfx::textdatum_t::middle_center);
  if (!isDrawn) {
    // Suele ser un JPEG progresivo: el decodificador solo lee los baseline
    logMsg(TAG, "%s no es un JPEG valido (progresivo?)", names[i]);
    drawNotice(canvas, BAD_PHOTO_TEXT, isOnline);
  }
}
