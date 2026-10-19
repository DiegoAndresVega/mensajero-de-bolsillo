#include "notifications.h"

#include "state.h"

namespace {
// Grosor del pliegue de la solapa
constexpr int32_t FLAP_STROKE = 3;
// Abanico del WiFi: arcos de 225 a 315 grados (0 = derecha, sentido horario)
constexpr float WIFI_ARC_START = 225.0F;
constexpr float WIFI_ARC_END = 315.0F;
constexpr uint8_t WIFI_ARC_COUNT = 3;
constexpr float OFFLINE_SLASH_WIDTH = 2.5F;

bool hasBaseline = false;
char lastId[MSG_ID_MAX_LEN + 1] = "";
}  // namespace

// Llegada = cambia ultimoId (solo lo cambia la tarea de red al guardar un
// mensaje). No se mira noLeidos: tambien sube al restaurar de la papelera un
// mensaje sin leer, y eso no es un mensaje nuevo.
NotificationStatus notificationsPoll() {
  const SystemState state = stateSnapshot();
  const bool hasNewArrival = hasBaseline && strcmp(state.ultimoId, lastId) != 0;
  hasBaseline = true;
  strlcpy(lastId, state.ultimoId, sizeof(lastId));
  return {state.noLeidos, hasNewArrival};
}

void notificationsDrawIcon(LGFX_Sprite& canvas, int32_t x, int32_t y, int32_t w, int32_t h, uint32_t color,
                           uint32_t bgColor, bool hasBadge) {
  const int32_t radius = h / 8;
  canvas.fillRoundRect(x, y, w, h, radius, color);
  // Solapa: una V del color de fondo sobre el cuerpo del sobre
  const int32_t tipX = x + w / 2;
  const int32_t tipY = y + h * 11 / 20;
  canvas.fillTriangle(x, y, x + w - 1, y, tipX, tipY, bgColor);
  canvas.fillTriangle(x + FLAP_STROKE, y, x + w - 1 - FLAP_STROKE, y, tipX, tipY - FLAP_STROKE, color);
  if (hasBadge) {
    const int32_t badgeRadius = h / 4;
    canvas.fillCircle(x + w, y, badgeRadius + 2, bgColor);
    canvas.fillCircle(x + w, y, badgeRadius, NOTIFICATION_BADGE_COLOR);
  }
}

void notificationsDrawOfflineIcon(LGFX_Sprite& canvas, int32_t cx, int32_t cy, int32_t size, uint32_t color,
                                  uint32_t bgColor) {
  const int32_t half = size / 2;
  // El vertice del abanico queda abajo, centrado
  const int32_t baseY = cy + half - 2;
  const int32_t step = size / (2 * WIFI_ARC_COUNT + 1);
  canvas.fillRect(cx - half, cy - half, size, size, bgColor);
  canvas.fillCircle(cx, baseY, step / 2 + 1, color);
  for (uint8_t i = 1; i <= WIFI_ARC_COUNT; ++i) {
    const int32_t outer = step * 2 * i + step / 2;
    canvas.fillArc(cx, baseY, outer, outer - step + 1, WIFI_ARC_START, WIFI_ARC_END, color);
  }
  canvas.drawWideLine(cx - half + 2, cy - half + 2, cx + half - 2, cy + half - 2, OFFLINE_SLASH_WIDTH,
                      NOTIFICATION_BADGE_COLOR);
}
