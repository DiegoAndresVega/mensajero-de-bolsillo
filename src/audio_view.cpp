#include "audio_view.h"

#include "audio_view_rules.h"
#include "config.h"
#include "fonts/fonts.h"
#include "menu.h"
#include "storage.h"
#include "trash_rules.h"
#include "ui_icons.h"
#include "ui_style.h"

namespace {
constexpr const char* FOOTER_PLAYING = "";
constexpr const char* FOOTER_STOPPED = "";
constexpr const char* FOOTER_TRASH_STOPPED = "";
constexpr size_t DATE_TEXT_MAX = 24;
constexpr size_t TIME_TEXT_MAX = 16;
constexpr size_t VOLUME_TEXT_MAX = 4;
// Caracteres del titulo en su linea; lo que sobra se corta con "..."
constexpr size_t TITLE_MAX_CHARS = 26;

// Layout de 280x240: cabecera con el autor (0-40), titulo, fecha, onda,
// play/pausa con el tiempo y pie
constexpr int32_t TITLE_Y = 56;
constexpr int32_t DATE_Y = 76;
constexpr int32_t WAVES_TOP = 90;
constexpr int32_t WAVES_HEIGHT = 84;
constexpr int32_t WAVES_WIDTH =
    static_cast<int32_t>(AUDIO_VIEW_BARS) * (AUDIO_VIEW_BAR_WIDTH + AUDIO_VIEW_BAR_GAP) - AUDIO_VIEW_BAR_GAP;
constexpr int32_t WAVES_LEFT = (LCD_WIDTH - WAVES_WIDTH) / 2;
constexpr int32_t WAVES_MID_Y = WAVES_TOP + WAVES_HEIGHT / 2;
constexpr int32_t BAR_MIN_HEIGHT = 4;
constexpr int32_t BAR_MAX_HEIGHT = WAVES_HEIGHT - 4;
constexpr int32_t CONTROLS_TOP = 182;
constexpr int32_t CONTROLS_HEIGHT = 34;
constexpr int32_t CONTROLS_MID_Y = CONTROLS_TOP + CONTROLS_HEIGHT / 2;
constexpr int32_t PLAY_ICON_X = 60;
constexpr int32_t PLAY_ICON_RADIUS = 15;
constexpr int32_t TIME_X = PLAY_ICON_X + PLAY_ICON_RADIUS + 12;
constexpr int32_t FOOTER_Y = 229;
// Barra de volumen: en lugar del titulo y la fecha
constexpr int32_t VOLUME_TOP = 46;
constexpr int32_t VOLUME_HEIGHT = 40;
constexpr int32_t VOLUME_MID_Y = VOLUME_TOP + VOLUME_HEIGHT / 2;
constexpr int32_t VOLUME_ICON_W = 22;
constexpr int32_t VOLUME_ICON_H = 18;
constexpr int32_t VOLUME_TRACK_X = UI_SIDE_MARGIN + VOLUME_ICON_W + 12;
constexpr int32_t VOLUME_TRACK_W = LCD_WIDTH - VOLUME_TRACK_X - 44;
constexpr int32_t VOLUME_TRACK_H = 10;
constexpr int32_t VOLUME_TEXT_RIGHT_X = LCD_WIDTH - UI_SIDE_MARGIN;

// Onda: azul en las barras bajas y morado en las altas
constexpr uint32_t WAVE_LOW_COLOR = 0x4388D1U;   // RGB888
constexpr uint32_t WAVE_HIGH_COLOR = 0x8C43D1U;  // RGB888
constexpr uint32_t PCT = 100;

MessageSummary current;
bool isCurrentInTrash = false;
char path[AUDIO_PATH_MAX] = "";

void drawTitleAndDate(LGFX_Sprite& canvas) {
  char text[MSG_TITLE_SIZE];
  canvas.setTextSize(1);
  canvas.setTextDatum(lgfx::textdatum_t::middle_center);
  canvas.setFont(&FreeSansBold9pt8b);
  canvas.setTextColor(UI_TEXT_COLOR);
  truncateForRow(current.titulo, TITLE_MAX_CHARS, text, sizeof(text));
  canvas.drawString(text, LCD_WIDTH / 2, TITLE_Y);
  char date[DATE_TEXT_MAX];
  formatDate(current.fecha, true, date, sizeof(date));
  canvas.setFont(&FreeSans9pt8b);
  canvas.setTextColor(UI_DIM_COLOR);
  canvas.drawString(date, LCD_WIDTH / 2, DATE_Y);
}

void drawVolume(LGFX_Sprite& canvas, uint8_t volume) {
  uiDrawSpeaker(canvas, UI_SIDE_MARGIN, VOLUME_MID_Y - VOLUME_ICON_H / 2, VOLUME_ICON_W, VOLUME_ICON_H,
                UI_TEXT_COLOR);
  const int32_t trackY = VOLUME_MID_Y - VOLUME_TRACK_H / 2;
  canvas.fillRoundRect(VOLUME_TRACK_X, trackY, VOLUME_TRACK_W, VOLUME_TRACK_H, VOLUME_TRACK_H / 2,
                       UI_SEPARATOR_COLOR);
  const int32_t fill = volumeFillWidth(volume, VOLUME_TRACK_W);
  if (fill > 0) {
    canvas.fillRoundRect(VOLUME_TRACK_X, trackY, fill, VOLUME_TRACK_H, VOLUME_TRACK_H / 2, UI_ACCENT_PINK);
  }
  char text[VOLUME_TEXT_MAX];
  snprintf(text, sizeof(text), "%u", static_cast<unsigned>(volume));
  canvas.setTextSize(1);
  canvas.setFont(&FreeSansBold9pt8b);
  canvas.setTextColor(UI_TEXT_COLOR);
  canvas.setTextDatum(lgfx::textdatum_t::middle_right);
  canvas.drawString(text, VOLUME_TEXT_RIGHT_X, VOLUME_MID_Y);
}

// Circulo rosa con el simbolo de lo que hara el 3: pausa si suena, play si no
void drawPlayIcon(LGFX_Sprite& canvas, bool isPlaying) {
  const int32_t cx = PLAY_ICON_X;
  const int32_t cy = CONTROLS_MID_Y;
  const int32_t r = PLAY_ICON_RADIUS;
  canvas.fillCircle(cx, cy, r, UI_ACCENT_PINK);
  if (isPlaying) {
    const int32_t barW = r / 3;
    const int32_t barH = r;
    canvas.fillRect(cx - barW - barW / 2, cy - barH / 2, barW, barH, UI_BG_COLOR);
    canvas.fillRect(cx + barW / 2, cy - barH / 2, barW, barH, UI_BG_COLOR);
    return;
  }
  // El triangulo va un poco a la derecha para que parezca centrado
  const int32_t half = r / 2;
  canvas.fillTriangle(cx - half + 2, cy - half - 1, cx - half + 2, cy + half + 1, cx + half + 3, cy, UI_BG_COLOR);
}

void drawFooter(LGFX_Sprite& canvas, bool isPlaying) {
  if (isPlaying) {
    menuDrawFooter(canvas, FOOTER_PLAYING, FOOTER_Y);
  } else {
    menuDrawFooter(canvas, isCurrentInTrash ? FOOTER_TRASH_STOPPED : FOOTER_STOPPED, FOOTER_Y);
  }
}
}  // namespace

bool operator==(const AudioViewModel& a, const AudioViewModel& b) {
  return a.state == b.state && a.positionSec == b.positionSec && a.durationSec == b.durationSec &&
         a.isVolumeShown == b.isVolumeShown && a.volume == b.volume && a.isFavorite == b.isFavorite &&
         a.isOnline == b.isOnline;
}

void audioViewOpen(const MessageSummary& msg, bool isInTrash) {
  current = msg;
  isCurrentInTrash = isInTrash;
  if (isInTrash) {
    buildMessageFilePath(TRASH_AUDIOS_DIR, msg.id, ".mp3", path, sizeof(path));
  } else {
    strlcpy(path, audioPath(msg.id).c_str(), sizeof(path));
  }
}

const char* audioViewId() {
  return current.id;
}

const char* audioViewPath() {
  return path;
}

void audioViewSetFavorite(bool isFavorite) {
  current.isFavorite = isFavorite;
}

bool audioViewIsFavorite() {
  return current.isFavorite;
}

void audioViewDraw(LGFX_Sprite& canvas, const AudioViewModel& model, uint32_t waveTick) {
  char author[MSG_AUTHOR_SIZE];
  truncateForRow(current.autor, MSG_AUTHOR_MAX_CHARS, author, sizeof(author));
  menuDrawHeader(canvas, author, model.isOnline, OfflineIconSide::Left);
  uiDrawFavoriteBadge(canvas, model.isFavorite);
  if (model.isVolumeShown) {
    drawVolume(canvas, model.volume);
  } else {
    drawTitleAndDate(canvas);
  }
  audioViewDrawWaves(canvas, waveTick);
  audioViewDrawControls(canvas, model);
  drawFooter(canvas, model.state == PlayerState::Playing);
}

ScreenRect audioViewWavesRect() {
  return {WAVES_LEFT, WAVES_TOP, WAVES_WIDTH, WAVES_HEIGHT};
}

void audioViewDrawWaves(LGFX_Sprite& canvas, uint32_t waveTick) {
  canvas.fillRect(WAVES_LEFT, WAVES_TOP, WAVES_WIDTH, WAVES_HEIGHT, UI_BG_COLOR);
  for (uint32_t bar = 0; bar < AUDIO_VIEW_BARS; ++bar) {
    const int32_t h = waveBarHeight(bar, AUDIO_VIEW_BARS, waveTick, BAR_MIN_HEIGHT, BAR_MAX_HEIGHT);
    const auto heightPct = static_cast<uint32_t>((h - BAR_MIN_HEIGHT) * static_cast<int32_t>(PCT) /
                                                 (BAR_MAX_HEIGHT - BAR_MIN_HEIGHT));
    const uint32_t color = uiBlendRgb(WAVE_HIGH_COLOR, WAVE_LOW_COLOR, heightPct);
    const int32_t x = WAVES_LEFT + static_cast<int32_t>(bar) * (AUDIO_VIEW_BAR_WIDTH + AUDIO_VIEW_BAR_GAP);
    canvas.fillRoundRect(x, WAVES_MID_Y - h / 2, AUDIO_VIEW_BAR_WIDTH, h, AUDIO_VIEW_BAR_WIDTH / 2, color);
  }
}

ScreenRect audioViewControlsRect() {
  return {0, CONTROLS_TOP, LCD_WIDTH, CONTROLS_HEIGHT};
}

void audioViewDrawControls(LGFX_Sprite& canvas, const AudioViewModel& model) {
  canvas.fillRect(0, CONTROLS_TOP, LCD_WIDTH, CONTROLS_HEIGHT, UI_BG_COLOR);
  drawPlayIcon(canvas, model.state == PlayerState::Playing);
  char time[TIME_TEXT_MAX];
  formatPlayTime(model.positionSec, model.durationSec, time, sizeof(time));
  canvas.setTextSize(1);
  canvas.setFont(&FreeSansBold12pt8b);
  canvas.setTextColor(UI_TEXT_COLOR);
  canvas.setTextDatum(lgfx::textdatum_t::middle_left);
  canvas.drawString(time, TIME_X, CONTROLS_MID_Y);
}
