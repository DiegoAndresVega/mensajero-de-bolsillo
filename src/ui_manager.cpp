#include "ui_manager.h"

#include <esp_heap_caps.h>
#include <esp_random.h>

#include <algorithm>

#include "audio_player.h"
#include "audio_view.h"
#include "busy_icon.h"
#include "buttons.h"
#include "config.h"
#include "display.h"
#include "eyes.h"
#include "info_screen.h"
#include "log.h"
#include "menu.h"
#include "message_list.h"
#include "message_view.h"
#include "multi_select.h"
#include "notification_popup.h"
#include "notifications.h"
#include "photos.h"
#include "popup_rules.h"
#include "settings.h"
#include "storage.h"
#include "tasks.h"
#include "trash.h"
#include "ui_context.h"
#include "ui_rules.h"
#include "utils.h"
#include "wifi_manager.h"

namespace {
constexpr const char* TAG = "UI";
constexpr uint32_t FRAME_MS = 1000 / UI_FPS;
// Onda del visor de audio y aviso con el bus ocupado: menos fotogramas
constexpr uint32_t SLOW_FRAME_MS = 1000 / AUDIO_VIEW_FPS;
constexpr uint8_t COLOR_DEPTH = 16;
constexpr uint32_t OFF_SCREEN_COLOR = 0x000000U;  // RGB888

constexpr UiTimeouts TIMEOUTS{UI_BOOT_MS, UI_IDLE_TO_EYES_MS, UI_EYES_TO_OFF_MS, UI_VIEW_IDLE_TO_EYES_MS};
// Igual, pero en Info (mensaje personal) y en Fotos se espera mas antes de ir a los ojos
constexpr UiTimeouts INFO_TIMEOUTS{UI_BOOT_MS, UI_INFO_IDLE_TO_EYES_MS, UI_EYES_TO_OFF_MS, UI_VIEW_IDLE_TO_EYES_MS};
// "Conectando..." maximo en Ajustes > WiFi: una vuelta por todas las redes,
// nunca menos de SETTINGS_WIFI_TIMEOUT_MS. Se fija en uiInit.
uint32_t wifiConnectingMaxMs = SETTINGS_WIFI_TIMEOUT_MS;

// Framebuffer completo en PSRAM (280x240x2 = 134400 bytes)
LGFX_Sprite canvas;

const char* stateName(UiState state) {
  switch (state) {
    case UiState::BOOT:
      return "BOOT";
    case UiState::EYES:
      return "EYES";
    case UiState::MENU:
      return "MENU";
    case UiState::MESSAGE_LIST:
      return "MESSAGE_LIST";
    case UiState::MESSAGE_VIEW:
      return "MESSAGE_VIEW";
    case UiState::AUDIO_VIEW:
      return "AUDIO_VIEW";
    case UiState::SETTINGS:
      return "SETTINGS";
    case UiState::INFO:
      return "INFO";
    case UiState::PHOTOS:
      return "PHOTOS";
    case UiState::MULTI_DELETE:
      return "MULTI_DELETE";
    case UiState::TRASH:
      return "TRASH";
    case UiState::OFF:
      return "OFF";
  }
  return "?";
}

// ----- Transiciones -----------------------------------------------------------

void enterState(UiContext& ctx, UiState next, uint32_t nowMs) {
  if (next == ctx.state) {
    return;
  }
  logMsg(TAG, "%s -> %s", stateName(ctx.state), stateName(next));
  // Un dialogo o una pulsacion para borrar no sobreviven a un cambio de pantalla
  deleteFlowClose();
  ctx.isHolding = false;
  // El audio solo suena en su visor: al salir se para y se olvida la pausa
  if (ctx.state == UiState::AUDIO_VIEW) {
    audioStop();
  }
  if (next == UiState::OFF) {
    // Negro antes de bajar el backlight: Wokwi no simula el PWM de GP17 y la
    // imagen seguiria viendose. Al despertar, needsFullPush repinta todo.
    display().fillScreen(OFF_SCREEN_COLOR);
    display().setBrightness(0);
    logMsg(TAG, "pantalla apagada");
  } else if (ctx.state == UiState::OFF) {
    display().setBrightness(LCD_BRIGHTNESS);
    logMsg(TAG, "pantalla encendida");
  }
  // Al despertar se empieza por arriba del menu; al volver de Mensajes se
  // conserva la seleccion
  const bool isWaking = ctx.state == UiState::BOOT || ctx.state == UiState::EYES || ctx.state == UiState::OFF;
  if (next == UiState::MENU && isWaking) {
    ctx.menu = MenuModel{};
    // Si hay algo en la web, que llegue ahora y no en el siguiente ciclo
    if (ctx.state != UiState::BOOT) {
      tasksRequestSync();
    }
  }
  if (next == UiState::EYES) {
    ctx.eyeSeed = esp_random();
    ctx.hasLoggedMood = false;
  }
  // Desde el menu se empieza por arriba; al volver del visor se conserva la
  // seleccion (en Favoritos puede haber una fila menos)
  if (next == UiState::MESSAGE_LIST) {
    if (ctx.state == UiState::MENU) {
      ctx.list = MessageListModel{};
    }
    messageListSetFavoritesOnly(ctx.isFavoritesOnly);
    ctx.list = messageListClamp(ctx.list, messageListCount(), MSG_ROWS_VISIBLE);
    ctx.needsListLoad = messageListNeedsLoad();
  }
  if (next == UiState::MESSAGE_VIEW || next == UiState::INFO) {
    ctx.viewTop = 0;
  }
  if (next == UiState::AUDIO_VIEW) {
    ctx.waveTick = 0;
    ctx.hasVolumeChange = false;
  }
  if (next == UiState::SETTINGS) {
    // Al volver de Borrado multiple se conserva la fila
    if (ctx.state == UiState::MENU) {
      ctx.settings = SettingsModel{};
    }
    ctx.hasVolumeBig = false;
    ctx.isWifiReconnecting = false;
  }
  if (next == UiState::MULTI_DELETE) {
    messageListSetFavoritesOnly(false);
    ctx.needsListLoad = messageListNeedsLoad();
    // Al volver del visor (revisar un mensaje) la seleccion sigue igual
    const bool isFromViewer = ctx.state == UiState::MESSAGE_VIEW || ctx.state == UiState::AUDIO_VIEW;
    if (!isFromViewer || ctx.needsListLoad) {
      multiSelectOpen(MultiSelectSource::Messages);
    }
  }
  // Desde el menu principal se lee la SD; al volver del visor se conserva donde se estaba
  if (next == UiState::TRASH && ctx.state == UiState::MENU) {
    trashScreenEnter();
  }
  ctx.state = next;
  ctx.enteredMs = nowMs;
  ctx.hasSurprise = false;
  ctx.needsFullPush = true;
}

// ----- Aviso de mensaje nuevo ----------------------------------------------------

// Aparece encima de cualquier pantalla. Con la pantalla apagada se enciende
// en los ojos. Si suena un mensaje, la tarea de audio lo pausa al llegar la
// notificacion (queda "interrumpido").
void startPopup(UiContext& ctx, uint32_t nowMs) {
  if (ctx.state == UiState::OFF) {
    enterState(ctx, UiState::EYES, nowMs);
  }
  if (!ctx.isPopupShown) {
    logMsg(TAG, "aviso de mensaje nuevo");
  }
  // Un boton que se estaba manteniendo no cuenta: el aviso se lleva los botones
  ctx.isHolding = false;
  ctx.isPopupShown = true;
  ctx.popupStartMs = nowMs;
  ctx.popupFrameMs = nowMs - SLOW_FRAME_MS;
}

// Vuelve a la pantalla de debajo. El audio interrumpido sigue por donde iba.
// En los ojos se empieza de cero con la cara de sorpresa: la pantalla se
// acaba de encender o de usar.
void finishPopup(UiContext& ctx, uint32_t nowMs, bool isCancelled) {
  if (isCancelled) {
    audioStopNotification();
    logMsg(TAG, "aviso cancelado");
  }
  if (audioStatus().isInterrupted) {
    audioResume();
  }
  ctx.isPopupShown = false;
  ctx.needsFullPush = true;
  // La tarjeta no puede quedarse en pantalla hasta que se libere el bus
  ctx.mustRepaintNow = true;
  ctx.lastInputMs = nowMs;
  if (ctx.state == UiState::EYES) {
    ctx.enteredMs = nowMs;
    ctx.eyeSeed = esp_random();
    ctx.hasLoggedMood = false;
    ctx.hasSurprise = true;
    ctx.surpriseStartMs = nowMs;
  }
}

void handleButtons(UiContext& ctx, uint32_t nowMs) {
  ButtonEvent ev;
  while (buttonsPollEvent(ev)) {
    // Con el aviso encima los botones no llegan a la pantalla de debajo
    if (ctx.isPopupShown) {
      if (popupCancelsOn(ev)) {
        finishPopup(ctx, nowMs, true);
      }
      continue;
    }
    if (deleteFlowIsOpen()) {
      ctx.lastInputMs = nowMs;
      uiHandleDialogButton(ctx, ev, nowMs);
      continue;
    }
    switch (uiRouteButton(ctx.state, ev)) {
      case UiButtonRoute::Ignore:
        break;
      case UiButtonRoute::BootInput:
        ctx.hadBootInput = true;
        ctx.lastInputMs = nowMs;
        break;
      case UiButtonRoute::Wake:
        ctx.lastInputMs = nowMs;
        enterState(ctx, UiState::MENU, nowMs);
        break;
      case UiButtonRoute::Screen:
        ctx.lastInputMs = nowMs;
        uiHandleScreenButton(ctx, ev, nowMs);
        break;
    }
  }
}

void handleNotifications(UiContext& ctx, uint32_t nowMs) {
  const NotificationStatus status = notificationsPoll();
  ctx.unread = status.unread;
  if (!status.hasNewArrival) {
    return;
  }
  logMsg(TAG, "mensaje nuevo, %lu sin leer", static_cast<unsigned long>(status.unread));
  // Con la lista abierta se recarga al quitar el aviso, sin tener que salir y
  // volver a entrar. En el borrado multiple no: perderia las casillas marcadas.
  if (ctx.state == UiState::MESSAGE_LIST) {
    ctx.needsListLoad = true;
  }
  // En el arranque no sale (son 2 s). Con un dialogo abierto espera a que se
  // cierre: el aviso no corta una confirmacion.
  switch (popupStartFor(true, ctx.state == UiState::BOOT, deleteFlowIsOpen())) {
    case PopupStart::Show:
      startPopup(ctx, nowMs);
      return;
    case PopupStart::Defer:
      ctx.hasPendingPopup = true;
      logMsg(TAG, "aviso aplazado hasta cerrar el dialogo");
      return;
    case PopupStart::None:
      return;
  }
}

// El aviso aplazado sale en cuanto se cierra el dialogo
void showPendingPopup(UiContext& ctx, uint32_t nowMs) {
  if (ctx.hasPendingPopup && !deleteFlowIsOpen()) {
    ctx.hasPendingPopup = false;
    startPopup(ctx, nowMs);
  }
}

void handleTimers(UiContext& ctx, uint32_t nowMs) {
  if (ctx.isPopupShown) {
    if (isPopupOver(ctx.popupStartMs, nowMs, NOTIF_POPUP_DURATION_MS)) {
      finishPopup(ctx, nowMs, false);
    }
    // Mientras se ve el aviso no corre el tiempo de inactividad
    return;
  }
  deleteFlowPoll();
  uiUpdateHold(ctx, nowMs);
  // Escuchar un audio cuenta como actividad: no se va a los ojos mientras suena
  if (ctx.state == UiState::AUDIO_VIEW && audioStatus().state == PlayerState::Playing) {
    ctx.lastInputMs = nowMs;
  }
  const bool isLongIdle = ctx.state == UiState::INFO || ctx.state == UiState::PHOTOS;
  const UiState next = uiStateAfterTime(ctx.state, nowMs - ctx.enteredMs, nowMs - ctx.lastInputMs,
                                        ctx.hadBootInput, isLongIdle ? INFO_TIMEOUTS : TIMEOUTS);
  enterState(ctx, next, nowMs);
}

// ----- Dibujo -------------------------------------------------------------------

// La SD, la pantalla y el audio (que lee el MP3 de la SD) comparten el bus
// SPI. El mutex del HAL ya evita choques; esto solo evita que los fotogramas
// retrasen una transferencia larga (guardado, descarga de MP3, reproduccion).
bool isSpiBusy() {
  return storageIsBusy() || audioIsPlaying();
}

void pushFull() {
  canvas.pushSprite(&display(), 0, 0);
}

// Solo un rectangulo del canvas; el resto de la pantalla ya esta enviado
void pushRegion(int32_t x, int32_t y, int32_t w, int32_t h) {
  display().setClipRect(x, y, w, h);
  canvas.pushSprite(&display(), 0, 0);
  display().clearClipRect();
}

void pushRect(const ScreenRect& r) {
  pushRegion(r.x, r.y, r.w, r.h);
}

// Con el bus ocupado no se anima nada: se pone el icono una sola vez (un
// envio de 36x36, menos de 1 ms) y no se vuelve a intentar dibujar hasta que
// se libere. Los botones y los cambios de estado siguen funcionando.
void showBusyIcon(UiContext& ctx) {
  if (ctx.isBusyIconShown) {
    return;
  }
  busyIconPush();
  ctx.isBusyIconShown = true;
}

// Al liberarse el bus se tapa el icono con lo que habia en el canvas
void hideBusyIcon(UiContext& ctx) {
  if (!ctx.isBusyIconShown) {
    return;
  }
  pushRect(busyIconRect());
  ctx.isBusyIconShown = false;
}

EyeScene currentScene(const UiContext& ctx, uint32_t msInState) {
  if (ctx.state == UiState::BOOT) {
    return eyeBootScene(msInState, ctx.eyeSeed);
  }
  const uint32_t nowMs = ctx.enteredMs + msInState;
  const bool isSurprised = ctx.hasSurprise && !hasElapsed(ctx.surpriseStartMs, nowMs, EYE_SURPRISE_MS);
  return eyeSceneAt(msInState, isSurprised, ctx.eyeSeed);
}

// Una linea por cambio de cara: sirve para seguirlas en Wokwi
void logMoodChange(UiContext& ctx, EyeMood mood) {
  if (ctx.hasLoggedMood && mood == ctx.loggedMood) {
    return;
  }
  logMsg(TAG, "ojos: %s", eyeMoodName(mood));
  ctx.loggedMood = mood;
  ctx.hasLoggedMood = true;
}

void renderEyes(UiContext& ctx, uint32_t nowMs) {
  const uint32_t msInState = nowMs - ctx.enteredMs;
  const EyeScene scene = currentScene(ctx, msInState);
  logMoodChange(ctx, scene.mood);
  const EyeFrame frame = eyeFrameAt(scene, msInState, ctx.eyeSeed);
  if (!ctx.needsFullPush && frame == ctx.shownEyes) {
    return;
  }
  if (ctx.needsFullPush) {
    canvas.fillScreen(EYE_BG_COLOR);
  }
  eyesDraw(canvas, frame);
  if (ctx.needsFullPush) {
    pushFull();
  } else {
    const EyesBand band = eyesBand();
    pushRegion(band.x, band.y, band.w, band.h);
  }
  ctx.shownEyes = frame;
  ctx.needsFullPush = false;
}

// "Cargando..." en pantalla y lectura de la SD. Se llama con el bus libre;
// mientras lee, la propia UI tiene la SD.
void loadMessageList(UiContext& ctx, bool isOnline) {
  messageListDrawLoading(canvas, isOnline);
  pushFull();
  messageListLoad();
  ctx.list = messageListClamp(ctx.list, messageListCount(), MSG_ROWS_VISIBLE);
  ctx.needsListLoad = false;
  ctx.needsFullPush = true;
}

// Lo que se ve en Ajustes. "Conectando..." dura hasta que hay conexion o
// pasa una vuelta por las redes (wifiConnectingMaxMs); luego se puede volver a pulsar 3.
SettingsView settingsViewOf(UiContext& ctx, uint32_t nowMs, bool isOnline) {
  const WifiScreenStatus wifi = wifiScreenStatus(isOnline, ctx.isWifiReconnecting, nowMs - ctx.wifiRequestMs,
                                                 SETTINGS_WIFI_MIN_CONNECTING_MS, wifiConnectingMaxMs);
  if (ctx.isWifiReconnecting && wifi != WifiScreenStatus::Connecting) {
    ctx.isWifiReconnecting = false;
  }
  SettingsView view;
  view.model = ctx.settings;
  view.volume = audioGetVolume();
  view.isSoundOn = audioNotificationSoundOn();
  view.isOnline = isOnline;
  view.wifi = wifi;
  view.isVolumeBig = ctx.hasVolumeBig && !hasElapsed(ctx.volumeBigMs, nowMs, SETTINGS_VOLUME_BIG_MS);
  view.refreshTick = ctx.settings.screen == SettingsScreen::WIFI ? nowMs / UI_STATIC_REFRESH_MS : 0;
  return view;
}

void renderStatic(UiContext& ctx, uint32_t nowMs) {
  const bool isOnline = wifiConnected.load();
  const bool usesMessageList = ctx.state == UiState::MESSAGE_LIST || ctx.state == UiState::MULTI_DELETE;
  if (usesMessageList && ctx.needsListLoad) {
    loadMessageList(ctx, isOnline);
    if (ctx.state == UiState::MULTI_DELETE) {
      multiSelectOpen(MultiSelectSource::Messages);
    }
  }
  if (ctx.state == UiState::TRASH) {
    trashRefreshIfStale();
  }
  if (ctx.state == UiState::PHOTOS) {
    photosLoadOnce();
  }
  const SettingsView settings = ctx.state == UiState::SETTINGS ? settingsViewOf(ctx, nowMs, isOnline) : SettingsView{};
  uint32_t flowRevision = 0;
  if (ctx.state == UiState::MULTI_DELETE) {
    flowRevision = multiSelectRevision();
  } else if (ctx.state == UiState::TRASH) {
    flowRevision = trashRevision() + multiSelectRevision();
  }
  const StaticKey key{ctx.state,   ctx.menu,       ctx.unread, isOnline,    ctx.list, messageListRevision(),
                      ctx.viewTop, ctx.photoIndex, settings,   flowRevision};
  if (!ctx.needsFullPush && key == ctx.shownStatic) {
    return;
  }
  switch (ctx.state) {
    case UiState::MESSAGE_LIST:
      messageListDraw(canvas, ctx.list, isOnline);
      break;
    case UiState::MESSAGE_VIEW:
      messageViewDraw(canvas, ctx.viewTop, isOnline);
      break;
    case UiState::SETTINGS:
      settingsDraw(canvas, settings);
      break;
    case UiState::INFO:
      infoScreenDraw(canvas, ctx.viewTop, isOnline);
      break;
    case UiState::PHOTOS:
      photosDraw(canvas, ctx.photoIndex, isOnline);
      break;
    case UiState::MULTI_DELETE:
      multiSelectDraw(canvas, isOnline);
      break;
    case UiState::TRASH:
      trashScreenDraw(canvas, isOnline);
      break;
    default:
      menuDraw(canvas, ctx.menu, ctx.unread, isOnline);
      break;
  }
  pushFull();
  ctx.shownStatic = key;
  ctx.needsFullPush = false;
}

// Visor de audio. Mientras suena el bus esta ocupado por su propio MP3 y aun
// asi se anima: la SD y la pantalla se turnan en el bus (mutex del HAL) y la
// libreria tiene el MP3 en un buffer, asi que no hace falta pausarlo. Para
// no quitarle demasiado bus, la onda va a AUDIO_VIEW_FPS y solo se envian
// los rectangulos que cambian: la onda (~37 KB, unos 8 ms) y, una vez por
// segundo, la fila del tiempo. La pantalla entera solo si cambia algo mas.
void renderAudioView(UiContext& ctx, uint32_t nowMs) {
  const AudioStatus status = audioStatus();
  const bool isPlaying = status.state == PlayerState::Playing;
  if (isPlaying) {
    ctx.waveTick = nowMs / SLOW_FRAME_MS;
  }
  const bool isVolumeShown =
      ctx.hasVolumeChange && !hasElapsed(ctx.volumeChangedMs, nowMs, AUDIO_VIEW_VOLUME_SHOW_MS);
  const AudioViewModel model{status.state,     status.positionSec, status.durationSec,    isVolumeShown,
                             audioGetVolume(), audioViewIsFavorite(), wifiConnected.load()};
  AudioViewModel sameTime = model;
  sameTime.positionSec = ctx.shownAudio.positionSec;
  const bool isOnlyTimeChanged = model.positionSec != ctx.shownAudio.positionSec && sameTime == ctx.shownAudio;
  if (ctx.needsFullPush || (!(model == ctx.shownAudio) && !isOnlyTimeChanged)) {
    audioViewDraw(canvas, model, ctx.waveTick);
    pushFull();
    ctx.isBusyIconShown = false;
    ctx.shownAudio = model;
    ctx.shownWaveTick = ctx.waveTick;
    ctx.needsFullPush = false;
    return;
  }
  if (isOnlyTimeChanged) {
    audioViewDrawControls(canvas, model);
    pushRect(audioViewControlsRect());
    ctx.shownAudio = model;
  }
  if (ctx.waveTick != ctx.shownWaveTick) {
    audioViewDrawWaves(canvas, ctx.waveTick);
    pushRect(audioViewWavesRect());
    ctx.shownWaveTick = ctx.waveTick;
  }
}

// El aviso se dibuja encima de lo que ya hay en el canvas y solo se envia su
// tarjeta. Con el bus ocupado (suena la notificacion) va a AUDIO_VIEW_FPS.
void renderPopup(UiContext& ctx, uint32_t nowMs) {
  const uint32_t frameMs = isSpiBusy() ? SLOW_FRAME_MS : FRAME_MS;
  if (!hasElapsed(ctx.popupFrameMs, nowMs, frameMs)) {
    return;
  }
  ctx.popupFrameMs = nowMs;
  notificationPopupDraw(canvas, popupWobbleX(nowMs - ctx.popupStartMs, NOTIF_POPUP_WOBBLE_HZ, NOTIF_POPUP_WOBBLE_PX));
  pushRect(notificationPopupRect());
}

// Dialogo de borrado encima de lo que hay en el canvas: se envia entero solo
// cuando cambia (una vez por pregunta)
void renderDialog(UiContext& ctx) {
  const uint32_t revision = deleteFlowRevision();
  if (revision == ctx.shownDialogRevision) {
    return;
  }
  deleteFlowDraw(canvas);
  pushFull();
  ctx.isBusyIconShown = false;
  ctx.shownDialogRevision = revision;
}

void render(UiContext& ctx, uint32_t nowMs) {
  if (ctx.state == UiState::OFF) {
    // Con el backlight a 0 no se ve nada: al despertar se decide de nuevo
    ctx.isBusyIconShown = false;
    return;
  }
  // El aviso y el visor de audio animan aunque suene audio: es el suyo
  if (ctx.isPopupShown) {
    renderPopup(ctx, nowMs);
    return;
  }
  if (deleteFlowIsOpen()) {
    renderDialog(ctx);
    return;
  }
  if (ctx.state == UiState::AUDIO_VIEW) {
    renderAudioView(ctx, nowMs);
    return;
  }
  const bool mustRepaint = ctx.mustRepaintNow;
  ctx.mustRepaintNow = false;
  if (isSpiBusy() && !mustRepaint) {
    showBusyIcon(ctx);
    return;
  }
  hideBusyIcon(ctx);
  if (ctx.state == UiState::BOOT || ctx.state == UiState::EYES) {
    renderEyes(ctx, nowMs);
  } else {
    renderStatic(ctx, nowMs);
  }
}

void uiTask(void*) {
  UiContext ctx;
  ctx.eyeSeed = esp_random();
  ctx.enteredMs = millis();
  ctx.lastInputMs = ctx.enteredMs;
  logMsg(TAG, "estado inicial %s", stateName(ctx.state));
  TickType_t lastWake = xTaskGetTickCount();
  for (;;) {
    const uint32_t nowMs = millis();
    handleButtons(ctx, nowMs);
    handleNotifications(ctx, nowMs);
    showPendingPopup(ctx, nowMs);
    handleTimers(ctx, nowMs);
    render(ctx, nowMs);
    vTaskDelayUntil(&lastWake, pdMS_TO_TICKS(FRAME_MS));
  }
}

bool initDisplay() {
  if (!display().init()) {
    logMsg(TAG, "fallo al iniciar la pantalla");
    return false;
  }
  display().setRotation(LCD_ROTATION);
  display().setBrightness(LCD_BRIGHTNESS);
  // En PSRAM para dejar la SRAM interna a WiFi/TLS
  canvas.setPsram(true);
  canvas.setColorDepth(COLOR_DEPTH);
  if (canvas.createSprite(LCD_WIDTH, LCD_HEIGHT) == nullptr) {
    logMsg(TAG, "sin memoria para el framebuffer");
    return false;
  }
  // Los textos van en Latin-1 (fonts/fonts.h): cada byte es un caracter
  canvas.setAttribute(lgfx::utf8_switch, 0);
  logMsg(TAG, "framebuffer %u bytes en %s", static_cast<unsigned>(canvas.bufferLength()),
         esp_ptr_external_ram(canvas.getBuffer()) ? "PSRAM" : "SRAM interna (NO esperado)");
  // Pequeno (2,6 KB): en SRAM interna. Si no hay memoria se sigue sin icono.
  if (!busyIconInit()) {
    logMsg(TAG, "sin memoria para el icono de ocupado");
  }
  return true;
}
}  // namespace

void uiEnterState(UiContext& ctx, UiState next, uint32_t nowMs) {
  enterState(ctx, next, nowMs);
}

LGFX_Sprite& uiCanvas() {
  return canvas;
}

bool uiInit(const AppConfig& config) {
  if (!initDisplay()) {
    return false;
  }
  infoScreenInit(canvas, config.personalMessage.c_str());
  wifiConnectingMaxMs = std::max(SETTINGS_WIFI_TIMEOUT_MS, wifiScanMaxMs(config.wifiNetworkCount));
  // Sin botones la UI sigue sirviendo: ojos y apagado por inactividad
  if (!buttonsInit()) {
    logMsg(TAG, "botones no disponibles");
  }
  if (xTaskCreatePinnedToCore(uiTask, "ui", STACK_UI_TASK, nullptr, PRIO_UI_TASK, nullptr, CORE_APP) != pdPASS) {
    logMsg(TAG, "no se pudo crear la tarea");
    canvas.deleteSprite();
    return false;
  }
  return true;
}
