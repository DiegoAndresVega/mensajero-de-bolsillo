#include "settings.h"

#include <WiFi.h>

#include "config.h"
#include "fonts/fonts.h"
#include "menu.h"
#include "notifications.h"
#include "ui_icons.h"
#include "ui_style.h"

namespace {
constexpr const char* TITLE = "Ajustes";
constexpr const char* ITEM_LABELS[SETTINGS_ITEM_COUNT] = {"Volumen", "Sonido notificaciones", "WiFi",
                                                          "Borrado m\xFAltiple"};
constexpr const char* LIST_FOOTER = "";
constexpr const char* VOLUME_FOOTER = "";
constexpr const char* SOUND_FOOTER = "";
constexpr const char* WIFI_FOOTER = "Presiona OK para reconectar";
constexpr const char* SOUND_LABEL = "Sonido de notificaciones";
constexpr const char* SOUND_NOTE = "";
constexpr size_t VALUE_TEXT_MAX = 24;

// Colores RGB888 (los comunes en ui_style.h). Seleccion como en el menu principal.
constexpr uint32_t HIGHLIGHT_COLOR = 0x4643D1U;

// Lista: 4 filas, etiqueta a la izquierda y valor a la derecha
constexpr int32_t ROW_TOP = 43;
constexpr int32_t ROW_HEIGHT = 43;
constexpr int32_t ROW_PADDING = 3;
constexpr int32_t ROW_RADIUS = 10;
constexpr int32_t LABEL_X = UI_SIDE_MARGIN + 10;
constexpr int32_t VALUE_RIGHT_X = LCD_WIDTH - UI_SIDE_MARGIN - 10;
constexpr int32_t FOOTER_Y = 228;
static_assert(ROW_TOP + SETTINGS_ITEM_COUNT * ROW_HEIGHT <= FOOTER_Y - 10, "las filas de Ajustes pisan el pie");

// Volumen: numero grande arriba, barra y numero pequeno a su derecha
constexpr int32_t VOLUME_BIG_Y = 100;
constexpr float VOLUME_BIG_TEXT_SIZE = 3.0F;
// Sin cambio reciente, un altavoz en lugar del numero grande
constexpr int32_t VOLUME_ICON_SIZE = 48;
constexpr int32_t VOLUME_BAR_Y = 158;
constexpr int32_t VOLUME_BAR_X = UI_SIDE_MARGIN * 2;
constexpr int32_t VOLUME_BAR_W = LCD_WIDTH - VOLUME_BAR_X - 64;
constexpr int32_t VOLUME_BAR_H = 14;
constexpr int32_t VOLUME_NUMBER_RIGHT_X = LCD_WIDTH - UI_SIDE_MARGIN * 2;

// Sonido: texto y pastilla con ON/OFF
constexpr int32_t SOUND_LABEL_Y = 80;
constexpr int32_t SOUND_PILL_Y = 130;
constexpr int32_t SOUND_PILL_W = 120;
constexpr int32_t SOUND_PILL_H = 50;
constexpr float SOUND_PILL_TEXT_SIZE = 2.0F;
constexpr int32_t SOUND_NOTE_Y = 188;

void setText(LGFX_Sprite& canvas, const lgfx::GFXfont& font, uint32_t color, lgfx::textdatum_t datum,
             float size = 1.0F) {
  canvas.setFont(&font);
  canvas.setTextSize(size);
  canvas.setTextColor(color);
  canvas.setTextDatum(datum);
}

// ----- Lista ----------------------------------------------------------------------

void drawRowValue(LGFX_Sprite& canvas, SettingsItem item, const SettingsView& view, int32_t cy, uint32_t fg) {
  char text[VALUE_TEXT_MAX] = "";
  switch (item) {
    case SettingsItem::VOLUME:
      snprintf(text, sizeof(text), "%u", static_cast<unsigned>(view.volume));
      break;
    case SettingsItem::NOTIFICATION_SOUND:
      strlcpy(text, settingsOnOffLabel(view.isSoundOn), sizeof(text));
      break;
    case SettingsItem::WIFI:
      setText(canvas, FreeSansBold9pt8b, view.isOnline ? fg : NOTIFICATION_BADGE_COLOR,
              lgfx::textdatum_t::middle_right);
      canvas.drawString(view.isOnline ? "OK" : "X", VALUE_RIGHT_X, cy);
      return;
    case SettingsItem::MULTI_DELETE:
      return;
  }
  setText(canvas, FreeSansBold9pt8b, fg, lgfx::textdatum_t::middle_right);
  canvas.drawString(text, VALUE_RIGHT_X, cy);
}

void drawList(LGFX_Sprite& canvas, const SettingsView& view) {
  menuDrawHeader(canvas, TITLE, view.isOnline);
  for (uint8_t i = 0; i < SETTINGS_ITEM_COUNT; ++i) {
    const int32_t rowY = ROW_TOP + i * ROW_HEIGHT;
    const int32_t cy = rowY + ROW_HEIGHT / 2;
    const bool isSelected = i == view.model.selected;
    const uint32_t bg = isSelected ? HIGHLIGHT_COLOR : UI_BG_COLOR;
    const uint32_t fg = isSelected ? UI_TEXT_COLOR : UI_DIM_COLOR;
    if (isSelected) {
      canvas.fillRoundRect(UI_SIDE_MARGIN, rowY + ROW_PADDING, LCD_WIDTH - 2 * UI_SIDE_MARGIN,
                           ROW_HEIGHT - 2 * ROW_PADDING, ROW_RADIUS, bg);
    }
    setText(canvas, FreeSansBold9pt8b, fg, lgfx::textdatum_t::middle_left);
    canvas.drawString(ITEM_LABELS[i], LABEL_X, cy);
    drawRowValue(canvas, static_cast<SettingsItem>(i), view, cy, fg);
  }
  menuDrawFooter(canvas, LIST_FOOTER, FOOTER_Y);
}

// ----- Subpantallas -------------------------------------------------------------------

void drawVolume(LGFX_Sprite& canvas, const SettingsView& view) {
  menuDrawHeader(canvas, "Volumen", view.isOnline);
  char text[VALUE_TEXT_MAX];
  snprintf(text, sizeof(text), "%u", static_cast<unsigned>(view.volume));
  if (view.isVolumeBig) {
    setText(canvas, FreeSansBold12pt8b, UI_ACCENT_PINK, lgfx::textdatum_t::middle_center, VOLUME_BIG_TEXT_SIZE);
    canvas.drawString(text, LCD_WIDTH / 2, VOLUME_BIG_Y);
  } else {
    uiDrawSpeaker(canvas, LCD_WIDTH / 2 - VOLUME_ICON_SIZE / 2, VOLUME_BIG_Y - VOLUME_ICON_SIZE / 2,
                  VOLUME_ICON_SIZE, VOLUME_ICON_SIZE, UI_DIM_COLOR);
  }
  const int32_t barY = VOLUME_BAR_Y - VOLUME_BAR_H / 2;
  canvas.fillRoundRect(VOLUME_BAR_X, barY, VOLUME_BAR_W, VOLUME_BAR_H, VOLUME_BAR_H / 2, UI_SEPARATOR_COLOR);
  const int32_t fill = VOLUME_BAR_W * view.volume / AUDIO_VOLUME_MAX;
  if (fill > 0) {
    canvas.fillRoundRect(VOLUME_BAR_X, barY, fill, VOLUME_BAR_H, VOLUME_BAR_H / 2, UI_ACCENT_PINK);
  }
  setText(canvas, FreeSansBold12pt8b, UI_TEXT_COLOR, lgfx::textdatum_t::middle_right);
  canvas.drawString(text, VOLUME_NUMBER_RIGHT_X, VOLUME_BAR_Y);
  menuDrawFooter(canvas, VOLUME_FOOTER, FOOTER_Y);
}

void drawSound(LGFX_Sprite& canvas, const SettingsView& view) {
  menuDrawHeader(canvas, "Sonido", view.isOnline);
  setText(canvas, FreeSansBold9pt8b, UI_TEXT_COLOR, lgfx::textdatum_t::middle_center);
  canvas.drawString(SOUND_LABEL, LCD_WIDTH / 2, SOUND_LABEL_Y);
  const uint32_t pillColor = view.isSoundOn ? UI_ACCENT_PINK : UI_SEPARATOR_COLOR;
  canvas.fillRoundRect(LCD_WIDTH / 2 - SOUND_PILL_W / 2, SOUND_PILL_Y - SOUND_PILL_H / 2, SOUND_PILL_W, SOUND_PILL_H,
                       SOUND_PILL_H / 2, pillColor);
  setText(canvas, FreeSansBold12pt8b, UI_TEXT_COLOR, lgfx::textdatum_t::middle_center, SOUND_PILL_TEXT_SIZE);
  canvas.drawString(settingsOnOffLabel(view.isSoundOn), LCD_WIDTH / 2, SOUND_PILL_Y);
  setText(canvas, FreeSans9pt8b, UI_DIM_COLOR, lgfx::textdatum_t::middle_center);
  canvas.drawString(SOUND_NOTE, LCD_WIDTH / 2, SOUND_NOTE_Y);
  menuDrawFooter(canvas, SOUND_FOOTER, FOOTER_Y);
}

const char* wifiStatusText(WifiScreenStatus status) {
  switch (status) {
    case WifiScreenStatus::Connected:
      return "Conectado";
    case WifiScreenStatus::Connecting:
      return "Conectando...";
    case WifiScreenStatus::Disconnected:
      return "Sin conexion";
  }
  return "";
}

// SSID, IP y RSSI solo con conexion. En wokwi_mode el WiFi es simulado y
// salen vacios: se pone "-".
void drawWifi(LGFX_Sprite& canvas, const SettingsView& view) {
  menuDrawHeader(canvas, "WiFi", view.isOnline);
  menuDrawBodyLine(canvas, 0, "Estado", wifiStatusText(view.wifi));
  if (view.wifi == WifiScreenStatus::Connected) {
    char value[VALUE_TEXT_MAX];
    const String ssid = WiFi.SSID();
    strlcpy(value, ssid.isEmpty() ? "-" : ssid.c_str(), sizeof(value));
    menuDrawBodyLine(canvas, 1, "SSID", value);
    const IPAddress ip = WiFi.localIP();
    strlcpy(value, ip == IPAddress(0, 0, 0, 0) ? "-" : ip.toString().c_str(), sizeof(value));
    menuDrawBodyLine(canvas, 2, "IP", value);
    const int32_t rssi = WiFi.RSSI();
    if (rssi == 0) {
      strlcpy(value, "-", sizeof(value));
    } else {
      snprintf(value, sizeof(value), "%ld dBm", static_cast<long>(rssi));
    }
    menuDrawBodyLine(canvas, 3, "RSSI", value);
  }
  menuDrawFooter(canvas, WIFI_FOOTER, FOOTER_Y);
}
}  // namespace

bool operator==(const SettingsView& a, const SettingsView& b) {
  return a.model == b.model && a.volume == b.volume && a.isSoundOn == b.isSoundOn && a.isOnline == b.isOnline &&
         a.wifi == b.wifi && a.isVolumeBig == b.isVolumeBig && a.refreshTick == b.refreshTick;
}

void settingsDraw(LGFX_Sprite& canvas, const SettingsView& view) {
  switch (view.model.screen) {
    case SettingsScreen::LIST:
      drawList(canvas, view);
      return;
    case SettingsScreen::VOLUME:
      drawVolume(canvas, view);
      return;
    case SettingsScreen::NOTIFICATION_SOUND:
      drawSound(canvas, view);
      return;
    case SettingsScreen::WIFI:
      drawWifi(canvas, view);
      return;
  }
}
