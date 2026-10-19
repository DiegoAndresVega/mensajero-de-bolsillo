#pragma once

// Log thread-safe: varias tareas escriben en el mismo puerto serie y sin
// mutex las lineas saldrian mezcladas.

void logInit();
void logMsg(const char* tag, const char* fmt, ...) __attribute__((format(printf, 2, 3)));
