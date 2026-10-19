#include "audio_player.h"
#include "audio_view_rules.h"
#include "buttons.h"
#include "config.h"
#include "info_rules.h"
#include "info_screen.h"
#include "photo_rules.h"
#include "photos.h"
#include "message_list.h"
#include "message_view.h"
#include "multi_select.h"
#include "tasks.h"
#include "trash.h"
#include "ui_context.h"
#include "utils.h"

namespace {
// Texto o voz. origin: pantalla a la que vuelve el visor. isInTrash: el
// fichero esta en la papelera.
void openMessage(UiContext& ctx, const MessageSummary& msg, UiState origin, bool isInTrash, uint32_t nowMs) {
  ctx.viewerOrigin = origin;
  if (msg.isAudio) {
    audioViewOpen(msg, isInTrash);
    uiEnterState(ctx, UiState::AUDIO_VIEW, nowMs);
  } else {
    messageViewOpen(uiCanvas(), msg, isInTrash);
    uiEnterState(ctx, UiState::MESSAGE_VIEW, nowMs);
  }
}

// Al abrir un mensaje se marca como leido (solo la primera vez), sea texto o
// voz. Desde la papelera no: ahi solo se mira.
void openSelectedMessage(UiContext& ctx, uint32_t nowMs) {
  const MessageSummary msg = messageListAt(ctx.list.selected);
  if (!msg.isRead) {
    messageListMarkRead(ctx.list.selected);
  }
  openMessage(ctx, msg, UiState::MESSAGE_LIST, false, nowMs);
}

// Solo desde la lista de mensajes el visor marca favorito y borra; desde la
// papelera o la seleccion multiple solo se mira
bool isReviewOnly(const UiContext& ctx) {
  return ctx.viewerOrigin != UiState::MESSAGE_LIST;
}

void applyFlowNav(UiContext& ctx, FlowNav nav, uint32_t nowMs) {
  switch (nav) {
    case FlowNav::None:
      return;
    case FlowNav::ToMessageList:
      if (ctx.state == UiState::MESSAGE_LIST) {
        ctx.list = messageListClamp(ctx.list, messageListCount(), MSG_ROWS_VISIBLE);
      } else {
        uiEnterState(ctx, UiState::MESSAGE_LIST, nowMs);
      }
      return;
    case FlowNav::ToSettings:
      uiEnterState(ctx, UiState::SETTINGS, nowMs);
      return;
    case FlowNav::ToMainMenu:
      uiEnterState(ctx, UiState::MENU, nowMs);
      return;
    case FlowNav::ToTrashMenu:
      trashShowMenu();
      uiEnterState(ctx, UiState::TRASH, nowMs);
      return;
    case FlowNav::OpenTrashMessage:
      openMessage(ctx, trashSelected(), UiState::TRASH, true, nowMs);
      return;
    case FlowNav::OpenSelectedMessage: {
      const bool isInTrash = multiSelectSource() == MultiSelectSource::Trash;
      openMessage(ctx, multiSelectCurrent(), ctx.state, isInTrash, nowMs);
      return;
    }
  }
}

// Pantallas donde mantener 3 o 4 (UI_LONG_HOLD_MS) hace algo: lista y
// visores (4 borra), seleccion multiple (3 abre, 4 borra) y papelera (3
// restaura)
bool acceptsHold(const UiContext& ctx, Button button) {
  switch (ctx.state) {
    case UiState::MESSAGE_LIST:
      return button == Button::BTN4_BACK;
    case UiState::MESSAGE_VIEW:
    case UiState::AUDIO_VIEW:
      return button == Button::BTN4_BACK && !isReviewOnly(ctx);
    case UiState::MULTI_DELETE:
    case UiState::TRASH:
      return button == Button::BTN3_OK || button == Button::BTN4_BACK;
    default:
      return false;
  }
}

// La pulsacion larga llega a los BTN_LONG_PRESS_MS de pulsar: desde ahi se
// sigue mirando el GPIO hasta UI_LONG_HOLD_MS (uiUpdateHold)
bool startHold(UiContext& ctx, const ButtonEvent& ev, uint32_t nowMs) {
  if (!ev.longPress || !acceptsHold(ctx, ev.button)) {
    return false;
  }
  ctx.isHolding = true;
  ctx.holdButton = ev.button;
  ctx.holdStartMs = nowMs - BTN_LONG_PRESS_MS;
  return true;
}

// Un audio sonando se para antes de preguntar: su MP3 no puede estar abierto
// cuando se mueva a la papelera
void onHoldDone(UiContext& ctx, Button button, uint32_t nowMs) {
  switch (ctx.state) {
    case UiState::MESSAGE_LIST: {
      const uint16_t count = ctx.needsListLoad ? 0 : messageListCount();
      if (messageListHoldAsksDelete(button, count)) {
        deleteFlowAskDeleteOne(messageListAt(ctx.list.selected).id);
      }
      return;
    }
    case UiState::AUDIO_VIEW:
      audioStop();
      deleteFlowAskDeleteOne(audioViewId());
      return;
    case UiState::MESSAGE_VIEW:
      deleteFlowAskDeleteOne(messageViewId());
      return;
    case UiState::MULTI_DELETE:
      applyFlowNav(ctx, multiSelectOnHold(button), nowMs);
      return;
    case UiState::TRASH:
      applyFlowNav(ctx, trashScreenOnHold(button), nowMs);
      return;
    default:
      return;
  }
}

void showVolume(UiContext& ctx, uint32_t nowMs) {
  ctx.hasVolumeChange = true;
  ctx.volumeChangedMs = nowMs;
}

void handleAudioViewButton(UiContext& ctx, const ButtonEvent& ev, uint32_t nowMs) {
  switch (audioViewOnButton(audioStatus().state, ev)) {
    case AudioViewAction::None:
      return;
    case AudioViewAction::Play:
      audioPlay(audioViewPath());
      return;
    case AudioViewAction::Pause:
      audioPause();
      return;
    case AudioViewAction::Resume:
      audioResume();
      return;
    case AudioViewAction::Stop:
      audioStop();
      return;
    case AudioViewAction::Back:
      // uiEnterState para el audio al salir del visor
      uiEnterState(ctx, ctx.viewerOrigin, nowMs);
      return;
    // Solo cambian un atomico que lee la tarea de audio y encolan el
    // guardado en config.json: no bloquean la UI
    case AudioViewAction::VolumeUp:
      audioVolumeUp();
      showVolume(ctx, nowMs);
      return;
    case AudioViewAction::VolumeDown:
      audioVolumeDown();
      showVolume(ctx, nowMs);
      return;
    case AudioViewAction::ToggleFavorite:
      if (!isReviewOnly(ctx)) {
        audioViewSetFavorite(messageListToggleFavorite(audioViewId()));
      }
      return;
  }
}

void handleListButton(UiContext& ctx, const ButtonEvent& ev, uint32_t nowMs) {
  // Hasta que se cargue la lista (tras "Cargando...") solo sirve volver
  const uint16_t count = ctx.needsListLoad ? 0 : messageListCount();
  const MessageListStep step = messageListOnButton(ctx.list, ev, count, MSG_ROWS_VISIBLE);
  ctx.list = step.model;
  if (step.exit == MessageListExit::Open) {
    openSelectedMessage(ctx, nowMs);
  } else if (step.exit == MessageListExit::Back) {
    uiEnterState(ctx, UiState::MENU, nowMs);
  }
}

void handleSettingsButton(UiContext& ctx, const ButtonEvent& ev, uint32_t nowMs) {
  const SettingsStep step = settingsOnButton(ctx.settings, ev);
  ctx.settings = step.model;
  switch (step.action) {
    case SettingsAction::None:
      return;
    case SettingsAction::Exit:
      uiEnterState(ctx, UiState::MENU, nowMs);
      return;
    case SettingsAction::VolumeUp:
    case SettingsAction::VolumeDown:
      // Se aplica ya; audio_player lo guarda en config.json
      audioSetVolume(settingsVolumeAfter(audioGetVolume(), step.action == SettingsAction::VolumeUp,
                                         SETTINGS_VOLUME_STEP));
      ctx.hasVolumeBig = true;
      ctx.volumeBigMs = nowMs;
      return;
    case SettingsAction::ToggleSound:
      audioSetNotificationSound(settingsToggledSound(audioNotificationSoundOn()));
      return;
    case SettingsAction::ReconnectWifi:
      tasksRequestWifiReconnect();
      ctx.isWifiReconnecting = true;
      ctx.wifiRequestMs = nowMs;
      return;
    case SettingsAction::OpenMultiDelete:
      uiEnterState(ctx, UiState::MULTI_DELETE, nowMs);
      return;
  }
}

}  // namespace

void uiHandleScreenButton(UiContext& ctx, const ButtonEvent& ev, uint32_t nowMs) {
  if (ctx.state == UiState::MENU) {
    const MenuStep step = menuOnButton(ctx.menu, ev);
    ctx.menu = step.model;
    if (step.exit == MenuExit::ToEyes) {
      uiEnterState(ctx, UiState::EYES, nowMs);
    } else if (step.exit == MenuExit::ToMessages || step.exit == MenuExit::ToFavorites) {
      ctx.isFavoritesOnly = step.exit == MenuExit::ToFavorites;
      uiEnterState(ctx, UiState::MESSAGE_LIST, nowMs);
    } else if (step.exit == MenuExit::ToPhotos) {
      uiEnterState(ctx, UiState::PHOTOS, nowMs);
    } else if (step.exit == MenuExit::ToTrash) {
      uiEnterState(ctx, UiState::TRASH, nowMs);
    } else if (step.exit == MenuExit::ToSettings) {
      uiEnterState(ctx, UiState::SETTINGS, nowMs);
    } else if (step.exit == MenuExit::ToInfo) {
      uiEnterState(ctx, UiState::INFO, nowMs);
    }
    return;
  }
  if (ctx.state == UiState::INFO) {
    const InfoStep step = infoOnButton(ctx.viewTop, ev, infoScreenLineCount(), infoScreenVisibleLines(),
                                       MSG_CONTENT_SCROLL_STEP);
    ctx.viewTop = step.topLine;
    if (step.isBack) {
      uiEnterState(ctx, UiState::MENU, nowMs);
    }
    return;
  }
  if (ctx.state == UiState::PHOTOS) {
    const PhotoStep step = photosOnButton(ctx.photoIndex, ev, photosCount());
    ctx.photoIndex = step.index;
    if (step.isBack) {
      uiEnterState(ctx, UiState::MENU, nowMs);
    }
    return;
  }
  if (ctx.state == UiState::SETTINGS) {
    handleSettingsButton(ctx, ev, nowMs);
    return;
  }
  if (startHold(ctx, ev, nowMs)) {
    return;
  }
  if (ctx.state == UiState::MULTI_DELETE) {
    applyFlowNav(ctx, multiSelectOnButton(ev), nowMs);
    return;
  }
  if (ctx.state == UiState::TRASH) {
    applyFlowNav(ctx, trashScreenOnButton(ev), nowMs);
    return;
  }
  if (ctx.state == UiState::AUDIO_VIEW) {
    handleAudioViewButton(ctx, ev, nowMs);
    return;
  }
  if (ctx.state == UiState::MESSAGE_LIST) {
    handleListButton(ctx, ev, nowMs);
    return;
  }
  if (ctx.state == UiState::MESSAGE_VIEW) {
    const MessageViewStep step = messageViewOnButton(ctx.viewTop, ev, messageViewLineCount(),
                                                     messageViewVisibleLines(), MSG_CONTENT_SCROLL_STEP);
    ctx.viewTop = step.topLine;
    if (step.isToggleFavorite && !isReviewOnly(ctx)) {
      messageViewSetFavorite(messageListToggleFavorite(messageViewId()));
    }
    if (step.isBack) {
      uiEnterState(ctx, ctx.viewerOrigin, nowMs);
    }
  }
}

void uiHandleDialogButton(UiContext& ctx, const ButtonEvent& ev, uint32_t nowMs) {
  applyFlowNav(ctx, deleteFlowOnButton(ev), nowMs);
  // Al cerrarse el dialogo se repinta la pantalla de debajo (estaba oscurecida)
  if (!deleteFlowIsOpen()) {
    ctx.needsFullPush = true;
  }
}

// Se cuenta desde que se pulso; si se suelta antes de UI_LONG_HOLD_MS no
// pasa nada
void uiUpdateHold(UiContext& ctx, uint32_t nowMs) {
  if (!ctx.isHolding) {
    return;
  }
  const HoldState hold = holdStateAt(buttonsIsPressed(ctx.holdButton), ctx.holdStartMs, nowMs, UI_LONG_HOLD_MS);
  if (hold == HoldState::Waiting) {
    return;
  }
  ctx.isHolding = false;
  if (hold == HoldState::Done) {
    ctx.lastInputMs = nowMs;
    onHoldDone(ctx, ctx.holdButton, nowMs);
  }
}
