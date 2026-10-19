#pragma once

#include <cstddef>
#include <cstdint>

// Cola de borrado remoto: ids de mensajes de la papelera que hay que borrar
// en el servidor (DELETE) antes de borrarlos de la SD. Vive en RAM (con
// mutex) y se guarda en TRASH_QUEUE_FILE a traves de la tarea de storage.
// La vacia la tarea de red cuando hay conexion (deletionQueueRun). Las
// reglas puras estan en deletion_queue_rules.h.

// Mutex y lectura de TRASH_QUEUE_FILE. En setup(), despues de storageInit().
void deletionQueueInit();

uint16_t deletionQueueCount();
bool deletionQueueContains(const char* id);

// Cambios: se aplican en RAM y se pide a la tarea de storage que guarde el
// fichero. Add no guarda (para anadir muchos de golpe): llamar despues a
// deletionQueueSave.
bool deletionQueueAdd(const char* id);
void deletionQueueSave();
// Quita un id (restaurado desde la papelera) y guarda
void deletionQueueRemove(const char* id);
// Vacia la cola y borra el fichero. Los mensajes siguen en la papelera.
void deletionQueueCancel();

// Solo la tarea de storage: escribe la cola actual (o borra el fichero si
// esta vacia). Escritura atomica.
bool deletionQueueWriteFile();

// Solo la tarea de red, con conexion: DELETE de cada id, de uno en uno. Si
// sale bien borra el mensaje de la papelera y lo quita de la cola; si falla
// lo deja para la siguiente vez. Para si se pierde la conexion.
void deletionQueueRun();

// Para la UI: sube al terminar cada deletionQueueRun, haya hecho algo o no
uint32_t deletionQueueRunCount();
