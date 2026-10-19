#include "log.h"

#include <cstdarg>

#include "config.h"

namespace {
constexpr size_t LOG_LINE_MAX = 256;
constexpr TickType_t LOG_LOCK_TIMEOUT = pdMS_TO_TICKS(100);

SemaphoreHandle_t logMutex = nullptr;
}  // namespace

void logInit() {
  DEBUG_SERIAL.begin(SERIAL_BAUD);
  logMutex = xSemaphoreCreateMutex();
}

void logMsg(const char* tag, const char* fmt, ...) {
  char line[LOG_LINE_MAX];
  va_list args;
  va_start(args, fmt);
  vsnprintf(line, sizeof(line), fmt, args);
  va_end(args);

  // Si el mutex no esta libre a tiempo se imprime igualmente: mejor una linea
  // mezclada que perder el mensaje.
  const bool isLocked = logMutex != nullptr && xSemaphoreTake(logMutex, LOG_LOCK_TIMEOUT) == pdTRUE;
  DEBUG_SERIAL.printf("[%8lu][%s] %s\n", static_cast<unsigned long>(millis()), tag, line);
  if (isLocked) {
    xSemaphoreGive(logMutex);
  }
}
