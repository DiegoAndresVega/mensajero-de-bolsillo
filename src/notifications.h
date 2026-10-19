#pragma once

#include <cstdint>

#include "display.h"

// Estado de las notificaciones para la UI: lee noLeidos y ultimoId de state.
// Solo lo usa la tarea de UI.

// Rojo del punto de "hay mensajes" y del contador del menu (RGB888)
constexpr uint32_t NOTIFICATION_BADGE_COLOR = 0xE02020U;

struct NotificationStatus {
  uint32_t unread;
  // true en la primera consulta tras cambiar ultimoId: acaba de llegar algo
  bool hasNewArrival;
};

// Una vez por fotograma. La primera llamada toma la cuenta actual como
// referencia: lo que ya habia al arrancar no cuenta como llegada nueva.
NotificationStatus notificationsPoll();

// Sobre dibujado con rectangulos y triangulos, dentro del rectangulo
// (x, y, w, h). Con hasBadge pone un punto rojo en la esquina superior derecha.
void notificationsDrawIcon(LGFX_Sprite& canvas, int32_t x, int32_t y, int32_t w, int32_t h, uint32_t color,
                           uint32_t bgColor, bool hasBadge);

// "Sin conexion": abanico de WiFi con una raya roja encima, centrado en
// (cx, cy) dentro de un cuadrado de `size` pixeles.
void notificationsDrawOfflineIcon(LGFX_Sprite& canvas, int32_t cx, int32_t cy, int32_t size, uint32_t color,
                                  uint32_t bgColor);
