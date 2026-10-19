#include "buttons.h"

#include "config.h"
#include "log.h"

namespace {
constexpr const char* TAG = "BTN";
// Mismo orden que el enum Button
constexpr uint8_t BUTTON_PINS[BUTTON_COUNT] = {PIN_BTN1, PIN_BTN2, PIN_BTN3, PIN_BTN4};
constexpr const char* BUTTON_NAMES[BUTTON_COUNT] = {"BTN1", "BTN2", "BTN3", "BTN4"};

QueueHandle_t eventQueue = nullptr;

uint8_t readPressedMask() {
  uint8_t mask = 0;
  for (uint8_t i = 0; i < BUTTON_COUNT; ++i) {
    // Activos en bajo
    if (digitalRead(BUTTON_PINS[i]) == LOW) {
      mask |= static_cast<uint8_t>(1U << i);
    }
  }
  return mask;
}

void logEvent(const ButtonEvent& ev) {
  const char* kind = ev.longPress ? "larga" : (ev.doubleTap ? "doble toque" : "toque");
  logMsg(TAG, "%s %s", BUTTON_NAMES[static_cast<uint8_t>(ev.button)], kind);
}

void buttonsTask(void*) {
  ButtonDecoder decoder(ButtonTiming{BTN_DEBOUNCE_MS, BTN_DOUBLE_TAP_MS, BTN_LONG_PRESS_MS});
  TickType_t lastWake = xTaskGetTickCount();
  for (;;) {
    ButtonEvent ev;
    if (decoder.update(readPressedMask(), millis(), ev)) {
      logEvent(ev);
      // Si la UI no da abasto se pierde la pulsacion, no se bloquea la lectura
      if (xQueueSend(eventQueue, &ev, 0) != pdTRUE) {
        logMsg(TAG, "cola llena, se descarta la pulsacion");
      }
    }
    vTaskDelayUntil(&lastWake, pdMS_TO_TICKS(BTN_POLL_MS));
  }
}
}  // namespace

bool buttonsInit() {
  if (eventQueue != nullptr) {
    return true;
  }
  for (const uint8_t pin : BUTTON_PINS) {
    pinMode(pin, INPUT_PULLUP);
  }
  eventQueue = xQueueCreate(BUTTON_QUEUE_LEN, sizeof(ButtonEvent));
  if (eventQueue == nullptr) {
    logMsg(TAG, "no se pudo crear la cola");
    return false;
  }
  if (xTaskCreatePinnedToCore(buttonsTask, "buttons", STACK_BUTTONS_TASK, nullptr, PRIO_BUTTONS_TASK, nullptr,
                              CORE_APP) != pdPASS) {
    logMsg(TAG, "no se pudo crear la tarea");
    vQueueDelete(eventQueue);
    eventQueue = nullptr;
    return false;
  }
  return true;
}

bool buttonsPollEvent(ButtonEvent& out) {
  return eventQueue != nullptr && xQueueReceive(eventQueue, &out, 0) == pdTRUE;
}

bool buttonsIsPressed(Button button) {
  const auto index = static_cast<uint8_t>(button);
  return index < BUTTON_COUNT && digitalRead(BUTTON_PINS[index]) == LOW;
}
