#pragma once

// Estado de la tarea de UI compartido entre ui_manager.cpp (bucle, tiempos y
// dibujo) y ui_input.cpp (que hace cada boton en cada pantalla). Interno de
// la UI: nadie mas lo incluye.

#include <cstdint>

#include "audio_view.h"
#include "delete_flow.h"
#include "display.h"
#include "eye_shapes.h"
#include "settings.h"
#include "ui_rules.h"

// Lo que hay ahora en una pantalla estatica (menu, info, lista, visor).
// Si no cambia, no se vuelve a enviar.
struct StaticKey {
  UiState state;
  MenuModel menu;
  uint32_t unread;
  // Icono de "sin conexion" en la cabecera
  bool isOnline;
  MessageListModel list;
  // Cambia al recargar la lista o marcar un mensaje como leido
  uint32_t listRevision;
  uint16_t viewTop;
  // Foto que se ve en Fotos; en el resto de pantallas no cambia
  uint16_t photoIndex;
  // Ajustes; en el resto de pantallas se queda por defecto
  SettingsView settings;
  // Borrado multiple y papelera: sube con cada cambio; en el resto, 0
  uint32_t flowRevision;
};

inline bool operator==(const StaticKey& a, const StaticKey& b) {
  return a.state == b.state && a.menu == b.menu && a.unread == b.unread && a.isOnline == b.isOnline &&
         a.list == b.list && a.listRevision == b.listRevision && a.viewTop == b.viewTop &&
         a.photoIndex == b.photoIndex && a.settings == b.settings && a.flowRevision == b.flowRevision;
}

// Estado de la tarea de UI; solo lo toca ella
struct UiContext {
  UiState state = UiState::BOOT;
  uint32_t enteredMs = 0;
  uint32_t lastInputMs = 0;
  bool hadBootInput = false;
  MenuModel menu;
  // Lista de mensajes: seleccion y si hay que leer la SD antes de pintarla
  MessageListModel list;
  bool needsListLoad = false;
  // Mensajes o Favoritos
  bool isFavoritesOnly = false;
  // Primera linea visible del visor o de Info
  uint16_t viewTop = 0;
  // Foto que se ve en Fotos. Se conserva al salir: se vuelve a la misma.
  uint16_t photoIndex = 0;
  // Pantalla a la que vuelve el visor. Si no es MESSAGE_LIST (papelera o
  // seleccion multiple) el visor solo sirve para mirar: sin favorito ni borrar.
  UiState viewerOrigin = UiState::MESSAGE_LIST;
  // Boton que se esta manteniendo (UI_LONG_HOLD_MS) y desde cuando
  bool isHolding = false;
  Button holdButton = Button::NONE;
  uint32_t holdStartMs = 0;
  // Llego un mensaje con un dialogo abierto: el aviso sale al cerrarlo
  bool hasPendingPopup = false;
  // Ultimo dialogo de borrado enviado a la pantalla
  uint32_t shownDialogRevision = 0;
  uint32_t unread = 0;
  // Visor de audio: onda que se ve (avanza solo mientras suena), lo ultimo
  // enviado y cuando se cambio el volumen
  uint32_t waveTick = 0;
  uint32_t shownWaveTick = 0;
  AudioViewModel shownAudio{};
  bool hasVolumeChange = false;
  uint32_t volumeChangedMs = 0;
  // Aviso de mensaje nuevo encima de la pantalla actual
  bool isPopupShown = false;
  uint32_t popupStartMs = 0;
  uint32_t popupFrameMs = 0;
  // Repintar en el siguiente fotograma aunque el bus este ocupado
  bool mustRepaintNow = false;
  // Ajustes: pantalla abierta, numero grande del volumen y reconexion WiFi
  SettingsModel settings;
  bool hasVolumeBig = false;
  uint32_t volumeBigMs = 0;
  bool isWifiReconnecting = false;
  uint32_t wifiRequestMs = 0;
  bool hasSurprise = false;
  // El icono de ocupado esta ahora mismo en pantalla
  bool isBusyIconShown = false;
  uint32_t surpriseStartMs = 0;
  // Lo ultimo enviado a la pantalla
  bool needsFullPush = true;
  StaticKey shownStatic{};
  EyeFrame shownEyes{};
  // Semilla del azar de los ojos: nueva en cada entrada en BOOT o EYES
  uint32_t eyeSeed = 0;
  // Ultima cara registrada en el log
  EyeMood loggedMood = EyeMood::LOOKING;
  bool hasLoggedMood = false;
};

// ui_manager.cpp: cambio de pantalla (para el audio al salir de su visor,
// enciende o apaga la pantalla...) y el framebuffer
void uiEnterState(UiContext& ctx, UiState next, uint32_t nowMs);
LGFX_Sprite& uiCanvas();

// ui_input.cpp: un boton en la pantalla activa (menu, listas, visores...)
void uiHandleScreenButton(UiContext& ctx, const ButtonEvent& ev, uint32_t nowMs);
// Un boton con el dialogo de borrado abierto
void uiHandleDialogButton(UiContext& ctx, const ButtonEvent& ev, uint32_t nowMs);
// Una vez por fotograma: mantener 3 o 4 UI_LONG_HOLD_MS (borrar, restaurar,
// abrir desde la seleccion multiple)
void uiUpdateHold(UiContext& ctx, uint32_t nowMs);
