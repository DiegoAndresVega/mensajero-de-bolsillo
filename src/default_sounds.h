#pragma once

// Sonido de notificacion por defecto (assets/sonidos/notificacion.mp3, ver
// tools/sonido/preparar.sh), dentro del firmware. Si la SD no trae
// /sonidos/notificacion.mp3 lo copia; si ya hay uno (propio) no lo toca.
// Se llama desde setup(), antes de que arranquen las tareas.
void defaultSoundsInstall();
