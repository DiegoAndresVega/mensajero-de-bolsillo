#pragma once

#include <cstdint>

#include "display.h"
#include "ui_rules.h"

// Dibujo del menu principal. La navegacion (que boton hace
// que) es logica pura y esta en ui_rules.h (menuOnButton). Solo lo usa la
// tarea de UI.

// Pinta la pantalla completa: la lista con la fila de `model` seleccionada.
// unread > 0 pone el punto rojo en el icono de Mensajes y el contador a la
// derecha de la fila.
// Sin conexion (isOnline false) pone el icono de "sin WiFi" en la cabecera.
void menuDraw(LGFX_Sprite& canvas, const MenuModel& model, uint32_t unread, bool isOnline);

// Linea "etiqueta   valor" del cuerpo (Ajustes > WiFi). line 0 es la
// primera debajo de la cabecera.
void menuDrawBodyLine(LGFX_Sprite& canvas, uint8_t line, const char* label, const char* value);

// Partes comunes con la lista y el visor de mensajes. La cabecera borra la
// pantalla, pone el titulo y el separador y, sin conexion, el icono de "sin
// WiFi" a la derecha (a la izquierda en los visores, que tienen ahi el
// corazon de favorito). El pie es una linea de ayuda centrada en footerY.
enum class OfflineIconSide : uint8_t { Right, Left };
void menuDrawHeader(LGFX_Sprite& canvas, const char* title, bool isOnline,
                    OfflineIconSide side = OfflineIconSide::Right);
void menuDrawFooter(LGFX_Sprite& canvas, const char* text, int32_t footerY);
