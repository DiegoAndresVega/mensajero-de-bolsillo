#pragma once

// Logica de los botones sin dependencias de Arduino: se compila tambien en el
// entorno native para los tests (pio test -e native). buttons.cpp solo lee
// los GPIO y le pasa las muestras a ButtonDecoder.

#include <cstdint>

enum class Button : uint8_t { BTN1_UP, BTN2_DOWN, BTN3_OK, BTN4_BACK, NONE };
constexpr uint8_t BUTTON_COUNT = 4;

struct ButtonEvent {
  Button button = Button::NONE;
  bool doubleTap = false;
  bool longPress = false;
};

struct ButtonTiming {
  uint32_t debounceMs;
  uint32_t doubleTapMs;
  uint32_t longPressMs;
};

// Convierte muestras periodicas de los 4 pines en eventos:
// - Rebote: un cambio vale si el nivel se mantiene debounceMs.
// - Pulsacion corta: se emite al soltar, si duro menos de longPressMs.
// - Doble toque: segunda pulsacion corta del mismo boton que empieza menos de
//   doubleTapMs despues de la primera. La primera ya se emitio como toque simple.
// - Pulsacion larga: se emite una vez al llegar a longPressMs, sin esperar a
//   soltar; al soltar no sale nada mas.
// - Una tecla a la vez: mientras un boton esta pulsado se ignoran los demas,
//   y un boton que ya estaba pulsado al soltar el primero tampoco cuenta.
class ButtonDecoder {
 public:
  explicit ButtonDecoder(const ButtonTiming& timing) : timing_(timing) {}

  // pressedMask: bit i a 1 si el boton i esta pulsado (nivel ya invertido).
  // Como solo cuenta una tecla a la vez, hay como mucho un evento por muestra.
  bool update(uint8_t pressedMask, uint32_t nowMs, ButtonEvent& out) {
    uint8_t pressedEdges = 0;
    uint8_t releasedEdges = 0;
    debounce(pressedMask, nowMs, pressedEdges, releasedEdges);

    // Primero las sueltas: si en la misma muestra se suelta uno y se pulsa
    // otro, el nuevo ya encuentra el camino libre.
    bool hasEvent = false;
    if (active_ != NO_ACTIVE && (releasedEdges & maskOf(active_)) != 0) {
      hasEvent = onRelease(out);
      active_ = NO_ACTIVE;
    }
    if (active_ == NO_ACTIVE) {
      startPress(pressedEdges, nowMs);
    }
    if (!hasEvent) {
      hasEvent = checkLongPress(nowMs, out);
    }
    return hasEvent;
  }

 private:
  static constexpr uint8_t NO_ACTIVE = BUTTON_COUNT;

  struct Channel {
    bool raw = false;
    bool stable = false;
    uint32_t rawChangedMs = 0;
  };

  static uint8_t maskOf(uint8_t index) { return static_cast<uint8_t>(1U << index); }

  void debounce(uint8_t pressedMask, uint32_t nowMs, uint8_t& pressedEdges, uint8_t& releasedEdges) {
    for (uint8_t i = 0; i < BUTTON_COUNT; ++i) {
      Channel& ch = channels_[i];
      const bool raw = (pressedMask & maskOf(i)) != 0;
      if (raw != ch.raw) {
        ch.raw = raw;
        ch.rawChangedMs = nowMs;
      }
      if (ch.raw != ch.stable && nowMs - ch.rawChangedMs >= timing_.debounceMs) {
        ch.stable = ch.raw;
        if (ch.stable) {
          pressedEdges |= maskOf(i);
        } else {
          releasedEdges |= maskOf(i);
        }
      }
    }
  }

  void startPress(uint8_t pressedEdges, uint32_t nowMs) {
    for (uint8_t i = 0; i < BUTTON_COUNT; ++i) {
      if ((pressedEdges & maskOf(i)) != 0) {
        active_ = i;
        pressStartMs_ = nowMs;
        isLongSent_ = false;
        return;
      }
    }
  }

  bool onRelease(ButtonEvent& out) {
    if (isLongSent_) {
      return false;
    }
    const bool isDouble =
        hasLastTap_ && lastTapIndex_ == active_ && pressStartMs_ - lastTapStartMs_ < timing_.doubleTapMs;
    // Tras un doble toque se empieza de cero: un triple toque no da dos dobles
    hasLastTap_ = !isDouble;
    lastTapIndex_ = active_;
    lastTapStartMs_ = pressStartMs_;
    out = ButtonEvent{static_cast<Button>(active_), isDouble, false};
    return true;
  }

  bool checkLongPress(uint32_t nowMs, ButtonEvent& out) {
    if (active_ == NO_ACTIVE || isLongSent_ || nowMs - pressStartMs_ < timing_.longPressMs) {
      return false;
    }
    isLongSent_ = true;
    hasLastTap_ = false;
    out = ButtonEvent{static_cast<Button>(active_), false, true};
    return true;
  }

  ButtonTiming timing_;
  Channel channels_[BUTTON_COUNT];
  uint8_t active_ = NO_ACTIVE;
  uint32_t pressStartMs_ = 0;
  bool isLongSent_ = false;
  bool hasLastTap_ = false;
  uint8_t lastTapIndex_ = NO_ACTIVE;
  uint32_t lastTapStartMs_ = 0;
};
