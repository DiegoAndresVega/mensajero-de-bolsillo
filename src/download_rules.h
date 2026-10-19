#pragma once

// Logica pura de la descarga de un MP3: cuando termina, cuando se da por
// colgada y cuanto leer cada vez. Sin dependencias de Arduino: se compila
// tambien en el entorno native para los tests (pio test -e native).

#include <cstddef>
#include <cstdint>

#include "utils.h"

enum class DownloadStatus : uint8_t {
  InProgress,
  Complete,
  // Pasan stallMs sin que llegue ningun byte
  Stalled,
  // El servidor cerro antes de Content-Length, o sin mandar nada (o
  // Content-Length es 0)
  Truncated,
  // Pasa de maxBytes (Content-Length o lo recibido si no hay)
  TooBig,
};

struct DownloadProgress {
  size_t received;
  // Content-Length; negativo si no se conoce
  int32_t expected;
  uint32_t lastDataMs;
  uint32_t nowMs;
  // Conexion abierta o con datos aun por leer
  bool isOpen;
};

inline DownloadStatus downloadStatus(const DownloadProgress& p, size_t maxBytes, uint32_t stallMs) {
  const bool hasSize = p.expected >= 0;
  const bool isTooBig = hasSize ? static_cast<size_t>(p.expected) > maxBytes : p.received > maxBytes;
  if (isTooBig) {
    return DownloadStatus::TooBig;
  }
  // Un MP3 vacio no vale: se trata como cortado
  if (hasSize && p.expected == 0) {
    return DownloadStatus::Truncated;
  }
  if (hasSize && p.received >= static_cast<size_t>(p.expected)) {
    return DownloadStatus::Complete;
  }
  if (!p.isOpen) {
    // Sin Content-Length el final es el cierre de la conexion
    return !hasSize && p.received > 0 ? DownloadStatus::Complete : DownloadStatus::Truncated;
  }
  if (hasElapsed(p.lastDataMs, p.nowMs, stallMs)) {
    return DownloadStatus::Stalled;
  }
  return DownloadStatus::InProgress;
}

// Bytes a leer ahora: lo disponible, sin pasar del buffer ni de lo que falta
// para Content-Length (si se conoce)
inline size_t downloadChunkLen(size_t available, size_t bufferSize, size_t received, int32_t expected) {
  size_t len = available < bufferSize ? available : bufferSize;
  if (expected >= 0) {
    const size_t total = static_cast<size_t>(expected);
    const size_t remaining = received < total ? total - received : 0;
    len = len < remaining ? len : remaining;
  }
  return len;
}
