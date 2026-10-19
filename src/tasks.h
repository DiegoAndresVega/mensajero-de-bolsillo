#pragma once

#include "config.h"

// Arquitectura:
//   Core 0  tarea red      -> WiFi, consulta a la API, descarga de mensajes
//   Core 1  tarea storage  -> unica que escribe JSON en la SD (cola de peticiones)
//   Core 1  tarea debug    -> estado por serie + eventos de mensajes nuevos
//   Core 1  tarea audio    -> reproduccion por cola (vive en audio_player.cpp)
// Las colas desacoplan las tareas: la futura UI solo tendra que leer la cola
// de eventos de mensaje, igual que hace hoy la tarea de debug.

// `config` debe vivir durante todo el programa. Si hasNetwork es false no se
// crea la tarea de red (config ausente o invalida: modo offline).
void tasksStart(const AppConfig& config, bool hasNetwork);

// Piden a la tarea de storage que guarde el volumen o el sonido de
// notificaciones en config.json. No esperan respuesta; si las tareas aun no
// existen, no se guarda.
void tasksRequestSaveVolume(uint8_t volume);
void tasksRequestSaveNotificationSound(bool isOn);

// Despierta a la tarea de red para que intente conectar ya (wifiReconnect)
// en vez de esperar al siguiente ciclo. No espera: el resultado se ve en
// wifiConnected. Sin tarea de red (sin config) no hace nada.
void tasksRequestWifiReconnect();

// Papelera: la tarea de storage mueve los ficheros y ajusta noLeidos. No
// esperan respuesta.
void tasksRequestMoveToTrash(const char* id);
void tasksRequestRestoreFromTrash(const char* id);
// Borra un mensaje de la papelera para siempre y espera a que termine. Solo
// la tarea de red, tras el DELETE en el servidor.
bool tasksDeleteFromTrash(const char* id);
// Guarda la cola de borrado remoto (deletion_queue) en la SD
void tasksRequestSaveDeletionQueue();
// Despierta a la tarea de red para que ejecute la cola ya (sin forzar
// reconexion). Sin tarea de red no hace nada: se ejecutara al conectar.
void tasksRequestDeletionRun();
// Despierta a la tarea de red para que pida los mensajes nuevos ya, sin
// esperar al siguiente ciclo (al encender la pantalla). Sin tarea de red no hace nada.
void tasksRequestSync();

// Pide a la tarea de storage que marque el mensaje como leido en su JSON y,
// si no lo estaba, baje noLeidos y guarde estado.json. No espera respuesta.
void tasksRequestMarkRead(const char* id);

// Pide a la tarea de storage que ponga "favorito" en el JSON del mensaje. Se
// pasa el valor final, no "cambiar": repetir la peticion no lo deshace.
void tasksRequestSetFavorite(const char* id, bool isFavorite);
