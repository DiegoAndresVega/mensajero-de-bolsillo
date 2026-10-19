#pragma once

// Logica pura sin dependencias de Arduino: se compila tambien en el entorno
// native para los tests (pio test -e native).

#include <cstddef>
#include <cstdint>
#include <cstring>

// Espera antes del reintento numero `attempt` (0 = primer reintento):
// base, 2*base, 4*base... sin pasar de maxMs.
inline uint32_t backoffDelayMs(uint8_t attempt, uint32_t baseMs, uint32_t maxMs) {
  constexpr uint8_t MAX_SHIFT = 31;
  if (attempt >= MAX_SHIFT) {
    return maxMs;
  }
  const uint64_t delay = static_cast<uint64_t>(baseMs) << attempt;
  return delay > maxMs ? maxMs : static_cast<uint32_t>(delay);
}

// Han pasado al menos durationMs desde startMs. La resta sin signo sigue
// siendo correcta cuando millis() da la vuelta (cada ~49 dias).
inline bool hasElapsed(uint32_t startMs, uint32_t nowMs, uint32_t durationMs) {
  return nowMs - startMs >= durationMs;
}

// El ID se usa como nombre de fichero y dentro de URLs: solo se aceptan
// caracteres alfanumericos, '_' y '-'. Evita path traversal ("../") y
// caracteres que FAT no admite.
inline bool isValidMessageId(const char* id, size_t maxLen) {
  if (id == nullptr) {
    return false;
  }
  const size_t len = strnlen(id, maxLen + 1);
  if (len == 0 || len > maxLen) {
    return false;
  }
  for (size_t i = 0; i < len; ++i) {
    const char c = id[i];
    const bool isAllowed = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                           (c >= '0' && c <= '9') || c == '_' || c == '-';
    if (!isAllowed) {
      return false;
    }
  }
  return true;
}

// Codifica un caracter para query string segun RFC 3986 (unreserved se deja tal cual).
// Escribe 1 o 3 caracteres en `out` y devuelve cuantos escribio.
inline size_t urlEncodeChar(char c, char out[3]) {
  const bool isUnreserved = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                            (c >= '0' && c <= '9') || c == '-' || c == '_' ||
                            c == '.' || c == '~';
  if (isUnreserved) {
    out[0] = c;
    return 1;
  }
  static const char HEX_DIGITS[] = "0123456789ABCDEF";
  const auto value = static_cast<uint8_t>(c);
  out[0] = '%';
  out[1] = HEX_DIGITS[value >> 4];
  out[2] = HEX_DIGITS[value & 0x0F];
  return 3;
}
