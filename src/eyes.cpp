#include "eyes.h"

#include <algorithm>

#include "config.h"

namespace {
constexpr int32_t LEFT_EYE_X = LCD_WIDTH / 2 - EYE_SPACING / 2;
constexpr int32_t RIGHT_EYE_X = LCD_WIDTH / 2 + EYE_SPACING / 2;
constexpr int32_t PUPIL_RADIUS = PUPIL_DIAMETER / 2;
static_assert(EYE_CENTER_Y * 2 == LCD_HEIGHT, "los ojos van centrados en vertical");

constexpr int32_t PCT = 100;
// Parpado de "cansado": grosor de la linea y cuanto sobresale por los lados
constexpr int32_t LID_THICKNESS = 6;
constexpr int32_t LID_OVERHANG = 4;
// Grosor de los arcos ^ y de la linea de "dormido"
constexpr int32_t STROKE = 12;
// El arco ^ se apoya un poco por debajo del centro del ojo
constexpr int32_t ARC_DROP = EYE_HEIGHT / 6;
// Pupila de corazon: 1,3 veces el diametro de la pupila redonda
constexpr int32_t HEART_SIZE_PCT = 130;
// Margen del rectangulo de envio alrededor del ojo mas grande
constexpr int32_t BAND_MARGIN = 8;
constexpr int32_t MAX_EYE_WIDTH = EYE_WIDTH * EYE_MAX_SCALE_PCT / PCT;
constexpr int32_t MAX_EYE_HEIGHT = EYE_HEIGHT * EYE_MAX_SCALE_PCT / PCT;
static_assert(EYE_WIDTH / 2 * EYE_MAX_ARC_PCT / PCT - ARC_DROP + EYE_LAUGH_SHAKE_PX <= MAX_EYE_HEIGHT / 2,
              "el arco mas alto se saldria del rectangulo de envio");

int32_t scaled(int32_t value, int32_t pct) {
  return value * pct / PCT;
}

// Corazon de ancho `size`: dos lobulos y un triangulo hacia abajo. openPct lo
// aplasta en vertical, igual que a la pupila redonda al parpadear.
void drawHeart(LGFX_Sprite& canvas, int32_t cx, int32_t cy, int32_t size, int32_t openPct) {
  const int32_t r = size / 4;
  const int32_t lobeY = cy - scaled(r / 2, openPct);
  const int32_t lobeRadiusY = std::max<int32_t>(1, scaled(r, openPct));
  canvas.fillEllipse(cx - r, lobeY, r, lobeRadiusY, EYE_HEART_COLOR);
  canvas.fillEllipse(cx + r, lobeY, r, lobeRadiusY, EYE_HEART_COLOR);
  const int32_t baseY = lobeY + scaled(r / 3, openPct);
  const int32_t tipY = cy + scaled(size / 2 - r / 2, openPct);
  canvas.fillTriangle(cx - 2 * r, baseY, cx + 2 * r, baseY, cx, tipY, EYE_HEART_COLOR);
}

void drawPupil(LGFX_Sprite& canvas, int32_t px, int32_t py, const EyeShape& s) {
  if (s.pupil == PupilForm::HEART) {
    drawHeart(canvas, px, py, scaled(scaled(PUPIL_DIAMETER, HEART_SIZE_PCT), s.pupilPct), s.openPct);
    return;
  }
  const int32_t radius = scaled(PUPIL_RADIUS, s.pupilPct);
  canvas.fillEllipse(px, py, radius, std::max<int32_t>(1, scaled(radius, s.openPct)), PUPIL_COLOR);
}

// Parpado: tapa la parte de arriba del ojo y deja una linea en el borde
void drawLid(LGFX_Sprite& canvas, int32_t cx, int32_t cy, int32_t w, int32_t h, int32_t lidPct) {
  const int32_t top = cy - h / 2 - 1;
  const int32_t lidY = top + scaled(h, lidPct);
  const int32_t left = cx - w / 2 - LID_OVERHANG;
  const int32_t width = w + 2 * LID_OVERHANG;
  canvas.fillRect(left, top, width, lidY - top, EYE_BG_COLOR);
  canvas.fillRect(left, lidY - LID_THICKNESS / 2, width, LID_THICKNESS, EYE_COLOR);
}

// Ovalo con pupila; el parpadeo lo aplasta en vertical
void drawOpenEye(LGFX_Sprite& canvas, int32_t cx, const EyeShape& s) {
  const int32_t w = scaled(EYE_WIDTH, s.scalePct);
  const int32_t h = scaled(EYE_HEIGHT, s.scalePct);
  const int32_t cy = EYE_CENTER_Y + s.dy;
  canvas.fillEllipse(cx, cy, w / 2, std::max<int32_t>(1, scaled(h / 2, s.openPct)), EYE_COLOR);
  drawPupil(canvas, cx + s.pupilDx, cy + scaled(s.pupilDy, s.openPct), s);
  if (s.lidPct > 0) {
    drawLid(canvas, cx, cy, w, h, s.lidPct);
  }
}

// ^ : mitad de arriba de un anillo eliptico (arcPct = su altura), con los
// extremos redondeados
void drawArcEye(LGFX_Sprite& canvas, int32_t cx, const EyeShape& s) {
  const int32_t rx = scaled(EYE_WIDTH / 2, s.scalePct);
  const int32_t ry = std::max<int32_t>(STROKE + 1, scaled(rx, s.arcPct));
  const int32_t cy = EYE_CENTER_Y + ARC_DROP + s.dy;
  canvas.fillEllipse(cx, cy, rx, ry, EYE_COLOR);
  canvas.fillEllipse(cx, cy, rx - STROKE, ry - STROKE, EYE_BG_COLOR);
  canvas.fillRect(cx - rx - 1, cy + 1, 2 * rx + 3, ry + 1, EYE_BG_COLOR);
  canvas.fillCircle(cx - rx + STROKE / 2, cy, STROKE / 2, EYE_COLOR);
  canvas.fillCircle(cx + rx - STROKE / 2, cy, STROKE / 2, EYE_COLOR);
}

void drawLineEye(LGFX_Sprite& canvas, int32_t cx, const EyeShape& s) {
  const int32_t w = scaled(EYE_WIDTH, s.scalePct);
  canvas.fillRoundRect(cx - w / 2, EYE_CENTER_Y + s.dy - STROKE / 2, w, STROKE, STROKE / 2, EYE_COLOR);
}

void drawEye(LGFX_Sprite& canvas, int32_t cx, const EyeShape& s) {
  switch (s.form) {
    case EyeForm::OPEN:
      drawOpenEye(canvas, cx, s);
      return;
    case EyeForm::ARC:
      drawArcEye(canvas, cx, s);
      return;
    case EyeForm::LINE:
      drawLineEye(canvas, cx, s);
      return;
  }
}
}  // namespace

EyesBand eyesBand() {
  const int32_t x = LEFT_EYE_X - MAX_EYE_WIDTH / 2 - BAND_MARGIN;
  const int32_t y = EYE_CENTER_Y - MAX_EYE_HEIGHT / 2 - BAND_MARGIN;
  return {x, y, RIGHT_EYE_X - LEFT_EYE_X + MAX_EYE_WIDTH + 2 * BAND_MARGIN, MAX_EYE_HEIGHT + 2 * BAND_MARGIN};
}

void eyesDraw(LGFX_Sprite& canvas, const EyeFrame& frame) {
  const EyesBand band = eyesBand();
  canvas.fillRect(band.x, band.y, band.w, band.h, EYE_BG_COLOR);
  drawEye(canvas, LEFT_EYE_X, frame.left);
  drawEye(canvas, RIGHT_EYE_X, frame.right);
}
