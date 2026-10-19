#pragma once

#include <Arduino.h>

#include "audio_rules.h"
#include "deletion_queue_rules.h"
#include "eye_rules.h"
#include "wifi_rules.h"

// ---------------------------------------------------------------------------
// Puerto serie de debug
// En la placa real Serial es el USB CDC; en Wokwi el monitor escucha UART0.
// ---------------------------------------------------------------------------
#ifdef WOKWI_BUILD
#define DEBUG_SERIAL Serial0
#else
#define DEBUG_SERIAL Serial
#endif

constexpr uint32_t SERIAL_BAUD = 115200;

// ---------------------------------------------------------------------------
// Pines (mapeo verificado, no cambiar)
// Pines a evitar: 19/20 (USB) y 21 (LED WS2812 interno). 43/44 (UART0) se
// usan como salidas en la placa, donde el log va por USB CDC.
// ---------------------------------------------------------------------------
constexpr uint8_t PIN_I2S_DIN = 4;
constexpr uint8_t PIN_I2S_BCLK = 5;
constexpr uint8_t PIN_I2S_LRC = 6;
// En Wokwi 43/44 son el monitor serie (Serial0): SD_MODE y backlight van
// en 16/17 solo en la simulacion (14 es BTN4).
#ifdef WOKWI_BUILD
constexpr uint8_t PIN_I2S_SD_MODE = 16;
#else
constexpr uint8_t PIN_I2S_SD_MODE = 44;  // RX
#endif

constexpr uint8_t PIN_BTN1 = 8;
constexpr uint8_t PIN_BTN2 = 7;
constexpr uint8_t PIN_BTN3 = 9;
constexpr uint8_t PIN_BTN4 = 14;

// Bus SPI compartido entre pantalla y MicroSD
constexpr uint8_t PIN_SPI_MOSI = 11;
constexpr uint8_t PIN_SPI_SCK = 12;
constexpr uint8_t PIN_SPI_MISO = 13;

constexpr uint8_t PIN_SD_CS = 2;

constexpr uint8_t PIN_LCD_CS = 10;
constexpr uint8_t PIN_LCD_RST = 3;
constexpr uint8_t PIN_LCD_DC = 1;
#ifdef WOKWI_BUILD
constexpr uint8_t PIN_LCD_BLK = 17;
#else
constexpr uint8_t PIN_LCD_BLK = 43;  // TX
#endif

// ---------------------------------------------------------------------------
// Pantalla (ST7789V2 240x280; en Wokwi se simula con un ILI9341 240x320)
// ---------------------------------------------------------------------------
// Tamano fisico del cristal, en la orientacion nativa del controlador
constexpr uint16_t LCD_PANEL_WIDTH = 240;
constexpr uint16_t LCD_PANEL_HEIGHT = 280;
// Se usa en horizontal: la UI dibuja en 280 de ancho x 240 de alto
constexpr uint16_t LCD_WIDTH = LCD_PANEL_HEIGHT;
constexpr uint16_t LCD_HEIGHT = LCD_PANEL_WIDTH;
// El controlador ST7789 tiene RAM de 240x320: el cristal de 280 empieza en la fila 20
constexpr uint16_t LCD_MEMORY_HEIGHT = 320;
constexpr uint16_t LCD_OFFSET_Y = 20;
constexpr uint32_t LCD_SPI_FREQ_HZ = 40000000;
constexpr uint32_t LCD_BL_PWM_FREQ_HZ = 44100;
constexpr uint8_t LCD_BL_PWM_CHANNEL = 7;
constexpr uint8_t LCD_BRIGHTNESS = 140;
// Orientacion: 1 = girada 90 grados (apaisada, 280x240). Si en la placa sale
// al reves, probar 3. En Wokwi no sale bien con ninguna rotacion: el ILI9341
// simulado no implementa bien MADCTL (limitacion del simulador). En la placa
// real hay que confirmar este valor y offset_rotation (src/display.cpp)
// mirando la pantalla.
constexpr uint8_t LCD_ROTATION = 1;

// ---------------------------------------------------------------------------
// MicroSD
// ---------------------------------------------------------------------------
constexpr uint32_t SD_SPI_FREQ_HZ = 20000000;

constexpr const char* PATH_CONFIG = "/config.json";
constexpr const char* PATH_STATE = "/estado.json";
// Cola de borrados remotos pendientes: {"pendientes": ["id1", "id2"]}. Sin
// pendientes no existe. La papelera esta en /papelera (trash_rules.h).
constexpr const char* TRASH_QUEUE_FILE = "/cola_borrado.json";
constexpr const char* DIR_MESSAGES = "/mensajes";
constexpr const char* DIR_AUDIOS = "/audios";
constexpr const char* DIR_SOUNDS = "/sonidos";
// Fotos que se copian a mano (tools/fotos/preparar.sh)
constexpr const char* DIR_PHOTOS = "/fotos";

// Sufijo de los ficheros temporales usados en escrituras atomicas
constexpr const char* TMP_SUFFIX = ".tmp";

// ---------------------------------------------------------------------------
// Red
// ---------------------------------------------------------------------------
// Varias redes: WIFI_MAX_NETWORKS, WIFI_TRY_TIMEOUT_MS y WIFI_ALL_FAILED_DELAY_MS
// estan en wifi_rules.h.
// Espera entre reintentos HTTP: 1 s, 2 s, 4 s... sin pasar del maximo
constexpr uint32_t HTTP_BACKOFF_BASE_MS = 1000;
constexpr uint32_t HTTP_BACKOFF_MAX_MS = 30000;

constexpr uint32_t HTTP_TIMEOUT_MS = 10000;
constexpr uint8_t HTTP_MAX_RETRIES = 3;
// Una respuesta de /nuevos mayor que esto se considera anomala y se descarta
constexpr size_t API_MAX_RESPONSE_BYTES = 64 * 1024;
// Tambien sin Content-Length: se cuenta lo recibido (download_rules.h)
constexpr size_t AUDIO_MAX_BYTES = 4 * 1024 * 1024;
// Descarga de un MP3: se lee y escribe en la SD en trozos de este tamano
constexpr size_t AUDIO_DOWNLOAD_CHUNK_BYTES = 4096;
// Sin recibir ningun byte durante esto, la descarga se da por colgada
constexpr uint32_t AUDIO_DOWNLOAD_STALL_MS = 10000;
// Espera cuando aun no han llegado datos
constexpr uint32_t AUDIO_DOWNLOAD_POLL_MS = 5;
// Pausa tras cada trozo recibido. Sin ella la tarea de red no se bloquea
// nunca mientras llegan datos y el TWDT del core 0 (IDLE0) reinicia la placa.
constexpr TickType_t AUDIO_DOWNLOAD_YIELD_TICKS = 1;

constexpr const char* NTP_SERVER = "pool.ntp.org";

#ifdef WOKWI_BUILD
constexpr uint32_t NET_POLL_INTERVAL_MS = 30UL * 1000UL;
#else
// Lo que tarda como mucho en llegar un mensaje subido a la web (y uno mas al
// encender la pantalla, que pide una consulta). Cada consulta es una peticion
// HTTPS corta; el WiFi sigue conectado entre una y otra.
constexpr uint32_t NET_POLL_INTERVAL_MS = 60UL * 1000UL;
#endif

// Los IDs de Firestore autogenerados tienen 20 caracteres; se deja margen
constexpr size_t MSG_ID_MAX_LEN = 40;

// ---------------------------------------------------------------------------
// FreeRTOS
// ---------------------------------------------------------------------------
constexpr BaseType_t CORE_NET = 0;
constexpr BaseType_t CORE_APP = 1;

// TLS + JSON necesitan bastante pila en la tarea de red
constexpr uint32_t STACK_NET_TASK = 12 * 1024;
constexpr uint32_t STACK_STORAGE_TASK = 6 * 1024;
constexpr uint32_t STACK_DEBUG_TASK = 4 * 1024;

constexpr UBaseType_t PRIO_NET_TASK = 2;
constexpr UBaseType_t PRIO_STORAGE_TASK = 3;
constexpr UBaseType_t PRIO_DEBUG_TASK = 1;

constexpr UBaseType_t STORAGE_QUEUE_LEN = 8;
constexpr uint32_t STORAGE_REPLY_TIMEOUT_MS = 5000;

constexpr uint32_t DEBUG_PRINT_INTERVAL_MS = 5000;

// ---------------------------------------------------------------------------
// Audio (MAX98357A por I2S). Volumen y comandos en audio_rules.h
// ---------------------------------------------------------------------------
constexpr const char* PATH_NOTIFICATION_SOUND = "/sonidos/notificacion.mp3";

// La tarea de audio llama a audio.loop() cada tick mientras suena algo, con
// pila para el parseo de cabeceras ID3 que hace la libreria dentro de loop()
constexpr uint32_t STACK_AUDIO_TASK = 6 * 1024;
constexpr UBaseType_t PRIO_AUDIO_TASK = 2;
constexpr UBaseType_t AUDIO_QUEUE_LEN = 4;
constexpr uint32_t AUDIO_LOOP_INTERVAL_MS = 1;
constexpr uint32_t AUDIO_IDLE_POLL_MS = 100;
// Seguridad: nada suena mas de 5 minutos seguidos
constexpr uint32_t AUDIO_MAX_PLAY_MS = 5UL * 60UL * 1000UL;
// Amplificador en shutdown (SD_MODE# en LOW) tras este tiempo sin sonar
constexpr uint32_t AUDIO_AMP_IDLE_OFF_MS = 3UL * 1000UL;
// Margen tras sacar el MAX98357A de shutdown antes de mandarle audio
constexpr uint32_t AUDIO_AMP_WAKE_MS = 10;
#ifdef WOKWI_BUILD
// Duracion de un mensaje de voz simulado: da tiempo a ver las ondas, pausar,
// reanudar y cambiar el volumen
constexpr uint32_t AUDIO_SIM_DURATION_MS = 20000;
#endif

// ---------------------------------------------------------------------------
// Botones (pull-up interno, activos en bajo; rebote por software)
// ---------------------------------------------------------------------------
constexpr uint32_t BTN_POLL_MS = 10;
constexpr uint32_t BTN_DEBOUNCE_MS = 30;
constexpr uint32_t BTN_DOUBLE_TAP_MS = 500;
constexpr uint32_t BTN_LONG_PRESS_MS = 500;
constexpr uint32_t STACK_BUTTONS_TASK = 3 * 1024;
// Por encima de la UI para no perder muestras mientras se dibuja
constexpr UBaseType_t PRIO_BUTTONS_TASK = 3;
constexpr UBaseType_t BUTTON_QUEUE_LEN = 8;

// ---------------------------------------------------------------------------
// UI
// ---------------------------------------------------------------------------
constexpr const char* FW_VERSION = "0.1.0";

// 30 FPS: la zona de los ojos (~72 KB) tarda unos 15 ms a 40 MHz, menos de
// la mitad de cada fotograma de 33 ms. Con la SD o el audio ocupados no se anima.
constexpr uint32_t UI_FPS = 30;
constexpr uint32_t UI_BOOT_MS = 2000;
// Menu (o cualquier pantalla) sin tocar nada -> ojos
constexpr uint32_t UI_IDLE_TO_EYES_MS = 15000;
// En Info del menu principal (mensaje personal) y en Fotos: da tiempo a
// leerlo o mirarlas sin pulsar nada
constexpr uint32_t UI_INFO_IDLE_TO_EYES_MS = 30000;
// En el visor de un mensaje: da tiempo a leer sin pulsar nada
constexpr uint32_t UI_VIEW_IDLE_TO_EYES_MS = 30000;
// Tiempo en los ojos sin doble toque -> pantalla apagada
constexpr uint32_t UI_EYES_TO_OFF_MS = 60000;
// Las pantallas estaticas (menu, lista, Ajustes...) solo se envian si
// cambian. Ajustes > WiFi, ademas, se repinta a este ritmo para refrescar IP y RSSI
constexpr uint32_t UI_STATIC_REFRESH_MS = 1000;
// 8 KB: la UI lee la SD y parsea JSON al cargar la lista y abrir un mensaje
constexpr uint32_t STACK_UI_TASK = 8 * 1024;
constexpr UBaseType_t PRIO_UI_TASK = 1;

// Ojos. Coordenadas en pixeles de la pantalla de 280x240, centrados.
constexpr int16_t EYE_WIDTH = 84;     // diametro horizontal del ojo
constexpr int16_t EYE_HEIGHT = 100;   // diametro vertical (ovalo)
constexpr int16_t EYE_SPACING = 136;  // distancia entre los centros de los ojos
constexpr int16_t EYE_CENTER_Y = LCD_HEIGHT / 2;
constexpr int16_t PUPIL_DIAMETER = 34;
constexpr uint32_t EYE_COLOR = 0xFFFFFFU;    // RGB888
constexpr uint32_t PUPIL_COLOR = 0x000000U;  // RGB888
constexpr uint32_t EYE_BG_COLOR = 0x000000U;  // RGB888
constexpr uint32_t EYE_HEART_COLOR = 0xE0245EU;  // RGB888, pupilas de "enamorado"
// Caras, tiempos y movimientos de los ojos: eye_rules.h (logica pura)
static_assert(EYE_BLINK_CLOSED_MS * UI_FPS > 1000, "el ojo cerrado debe durar mas que un fotograma");
static_assert(EYE_BLINK_CLOSED_MS < EYE_BLINK_MS, "el parpadeo necesita tiempo para cerrar y abrir");
static_assert(EYE_SLEEP_FROM_MS < UI_EYES_TO_OFF_MS, "la fase de dormido debe llegar antes del apagado");
// Cara de sorpresa al llegar un mensaje estando en los ojos
constexpr uint32_t EYE_SURPRISE_MS = 3000;

// ---------------------------------------------------------------------------
// Lista y visor de mensajes (logica pura en message_rules.h)
// ---------------------------------------------------------------------------
// Filas de la lista a la vez. Con 4 caben autor + titulo + fecha en cada fila;
// si se sube a 5, bajar MSG_ROW_HEIGHT a 35 (las filas ocupan 176 px en total)
constexpr uint16_t MSG_ROWS_VISIBLE = 4;
// Alto de cada fila en pixeles
constexpr int32_t MSG_ROW_HEIGHT = 44;
static_assert(MSG_ROWS_VISIBLE * MSG_ROW_HEIGHT <= 176, "las filas no caben entre la cabecera y el pie");
// Escala de las fuentes de la lista y del visor (1.0 = FreeSans 9 pt tal cual).
// Por encima de 1 las letras se agrandan pixeladas
constexpr float MSG_ROW_FONT_SIZE = 1.0F;
// Caracteres del titulo y del autor en la fila; lo que sobra se corta con "..."
constexpr size_t MSG_TITLE_MAX_CHARS = 20;
constexpr size_t MSG_AUTHOR_MAX_CHARS = 16;
// Lineas que baja o sube el visor con cada pulsacion de 1/2
constexpr uint16_t MSG_CONTENT_SCROLL_STEP = 3;
// Mensajes que se cargan como mucho (si hay mas, se quedan los mas nuevos).
// Cada uno ocupa unos 160 bytes en PSRAM
constexpr uint16_t MSG_LIST_MAX = 200;
// Texto maximo que se muestra de un mensaje; lo que pase se corta
constexpr size_t MSG_CONTENT_MAX_CHARS = 2048;
static_assert(DELETION_QUEUE_CAPACITY >= MSG_LIST_MAX, "la cola de borrado debe caber toda la papelera");
// Mantener pulsado un boton este tiempo (mas que la pulsacion larga normal
// de BTN_LONG_PRESS_MS). Uno para todo:
// - visor: 4 pide borrar el mensaje
// - Papelera > Ver mensajes: 3 pide restaurar
// - seleccion multiple (Ajustes > Borrado multiple y Papelera > Seleccionar):
//   3 abre el mensaje para revisarlo y 4 pide borrar los marcados
constexpr uint32_t UI_LONG_HOLD_MS = 2000;
// Lado del corazon de favorito en la esquina superior derecha de los visores
// (en la lista se usa uno mas pequeno, MSG_ROW_HEART_SIZE)
constexpr int32_t FAVORITE_ICON_SIZE = 22;
constexpr int32_t MSG_ROW_HEART_SIZE = 12;

// ---------------------------------------------------------------------------
// Visor de audio (logica pura en audio_view_rules.h)
// ---------------------------------------------------------------------------
// Barras de la onda decorativa
constexpr uint32_t AUDIO_VIEW_BARS = 24;
// Ancho de cada barra y hueco entre barras, en pixeles. Con 24 barras la onda
// ocupa 24 * 6 + 23 * 3 = 213 px de los 280 de ancho
constexpr int32_t AUDIO_VIEW_BAR_WIDTH = 6;
constexpr int32_t AUDIO_VIEW_BAR_GAP = 3;
static_assert(AUDIO_VIEW_BARS * (AUDIO_VIEW_BAR_WIDTH + AUDIO_VIEW_BAR_GAP) <= LCD_WIDTH,
              "la onda no cabe a lo ancho");
// Fotogramas por segundo de la onda mientras suena el audio: la mitad que el
// resto de la UI, para dejar el bus SPI libre a la lectura del MP3
constexpr uint32_t AUDIO_VIEW_FPS = 15;
// Tiempo que se ve la barra de volumen tras pulsar 1 o 2
constexpr uint32_t AUDIO_VIEW_VOLUME_SHOW_MS = 2000;

// ---------------------------------------------------------------------------
// Aviso de mensaje nuevo: sobre en grande en el centro (popup_rules.h)
// ---------------------------------------------------------------------------
// Duracion del aviso; el boton 4 lo corta antes
constexpr uint32_t NOTIF_POPUP_DURATION_MS = 3000;
// Vibracion horizontal del sobre: vueltas por segundo y amplitud en pixeles
constexpr uint32_t NOTIF_POPUP_WOBBLE_HZ = 2;
constexpr int32_t NOTIF_POPUP_WOBBLE_PX = 2;

// ---------------------------------------------------------------------------
// Menu Ajustes (logica pura en settings_rules.h)
// ---------------------------------------------------------------------------
// Lo que sube o baja el volumen cada pulsacion de 1/2 en Ajustes > Volumen
constexpr uint8_t SETTINGS_VOLUME_STEP = 1;
// Tiempo minimo del "Conectando..." maximo tras pulsar reconectar. Con muchas
// redes se alarga a una vuelta completa (wifiScanMaxMs en wifi_rules.h).
constexpr uint32_t SETTINGS_WIFI_TIMEOUT_MS = 10000;
// "Conectando..." se ve al menos esto, aunque ya hubiera conexion
constexpr uint32_t SETTINGS_WIFI_MIN_CONNECTING_MS = 1000;
// Tiempo que se ve el numero grande tras cambiar el volumen
constexpr uint32_t SETTINGS_VOLUME_BIG_MS = 2000;

// ---------------------------------------------------------------------------
// Info del menu principal: mensaje personal (logica pura en info_rules.h)
// ---------------------------------------------------------------------------
// Caracteres del mensaje personal que se muestran; lo que pase se corta con "..."
constexpr size_t INFO_MAX_CHARS = 500;

// ---------------------------------------------------------------------------
// Fotos del menu principal (logica pura en photo_rules.h)
// ---------------------------------------------------------------------------
// Fotos que se muestran de /fotos; si hay mas, el resto se ignora
constexpr uint16_t PHOTOS_MAX = 50;
// Caracteres del nombre de una foto (sin la carpeta)
constexpr size_t PHOTO_NAME_MAX = 32;
// Tamano maximo de un JPEG: se lee entero a PSRAM antes de decodificarlo.
// Una foto preparada (280x240) ocupa 20-40 KB; una del movil sin preparar,
// varios MB.
constexpr size_t PHOTO_MAX_BYTES = 256 * 1024;

// ---------------------------------------------------------------------------
// Configuracion leida de /config.json (inmutable tras el arranque)
// ---------------------------------------------------------------------------
struct AppConfig {
  // Se prueban en este orden; 0 = ninguna (en wokwi_mode se simula igual)
  WifiNetwork wifiNetworks[WIFI_MAX_NETWORKS] = {};
  uint8_t wifiNetworkCount = 0;
  String apiUrl;
  String apiToken;
  bool wokwiMode = false;
  uint8_t audioVolume = AUDIO_VOLUME_DEFAULT;
  // notification_sound: si false, al llegar un mensaje sale el aviso en
  // pantalla pero no suena
  bool notificationSound = true;
  // personal_message: texto de Info en el menu principal (UTF-8 tal cual del JSON)
  String personalMessage;
};
