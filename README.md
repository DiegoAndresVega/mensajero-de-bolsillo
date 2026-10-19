# Mensajero de bolsillo

Firmware para un llavero con pantalla que recibe mensajes de texto y de voz
por WiFi, los guarda en una tarjeta MicroSD y los muestra en pantalla o los
reproduce por un altavoz. Pensado para alguien que no usa el móvil: solo
tiene 4 botones.

Cuando no se usa, la pantalla muestra unos ojos animados que cambian de cara
y, al rato, se apaga.

## Qué hace

- Descarga mensajes nuevos de un backend HTTPS cada minuto.
- Lista de mensajes, visor de texto con scroll y visor de audio (MP3).
- Favoritos, papelera con restaurar y vaciar, borrado múltiple.
- Aviso de mensaje nuevo con sonido.
- Fotos (JPEG) a pantalla completa desde la SD.
- Pantalla "Info" con un mensaje personal configurable.
- Ajustes: volumen, sonido de notificación y reconexión WiFi.
- Hasta 5 redes WiFi.
- Textos con tildes y ñ (Latin-1). Fechas en hora peninsular española.

## Hardware

| Pieza | Modelo |
|---|---|
| Placa | Waveshare ESP32-S3-Zero (4 MB flash, 2 MB PSRAM) |
| Pantalla | ST7789V2 240x280, SPI (se usa en horizontal) |
| Audio | Amplificador MAX98357A (I2S) + altavoz 8 Ω 2 W |
| Almacenamiento | MicroSD por SPI (comparte bus con la pantalla) |
| Botones | 4 pulsadores a GND (pull-up interno) |

Conexiones:

| GPIO | Función |
|---|---|
| 4 / 5 / 6 | I2S DIN / BCLK / LRC (MAX98357A) |
| 44 | SD_MODE del MAX98357A |
| 8 / 7 / 9 / 14 | Botones 1 / 2 / 3 / 4 |
| 11 / 12 / 13 | SPI MOSI / CLK / MISO (pantalla y SD) |
| 10 | CS pantalla |
| 2 | CS SD |
| 3 / 1 | RES / DC pantalla |
| 43 | Backlight pantalla (PWM) |

Todos los pines están en `src/config.h`. GPIO 43/44 son la UART0: el log
sale por USB CDC.

## Compilar y flashear

Necesitas [PlatformIO](https://platformio.org/) (extensión de VS Code o CLI).

```bash
pio run -e waveshare_esp32_s3_zero -t upload   # compilar y flashear
pio device monitor                             # ver el log
pio test -e native                             # tests en el ordenador
```

Entornos de `platformio.ini`:

- `waveshare_esp32_s3_zero`: la placa real.
- `wokwi`: simulador [Wokwi](https://wokwi.com/) para VS Code
  (`wokwi/diagram.json`). Simula la API y el audio; no hace falta backend.
- `native`: tests de la lógica pura (`test/`).

## Preparar la tarjeta SD

1. Formatea la MicroSD en FAT32.
2. Copia `sd_ejemplo/config.json` a la raíz de la SD.
3. Rellena tus redes WiFi, la URL del backend y el token.

```json
{
  "wifi_networks": [
    { "ssid": "NOMBRE_DE_TU_WIFI", "password": "CLAVE_DE_TU_WIFI" }
  ],
  "api_url": "https://backend.example.com",
  "api_token": "PON_AQUI_EL_TOKEN",
  "wokwi_mode": false,
  "audio_volume": 15,
  "notification_sound": true,
  "personal_message": "Texto que sale en la pantalla Info"
}
```

El resto de carpetas (`/mensajes`, `/audios`, `/papelera`, `/sonidos`) las
crea el firmware. Para las fotos, mira la sección "Fotos".

## Botones

| Dónde | 1 | 2 | 3 | 4 |
|---|---|---|---|---|
| Ojos / apagado | doble toque: encender | igual | igual | igual |
| Menú y listas | subir | bajar | entrar | volver |
| Visor de texto | subir | bajar | favorito | volver; mantener 2 s: borrar |
| Visor de audio | volumen + | volumen - | play/pausa; mantener: favorito | stop / volver; mantener 2 s: borrar |
| Fotos | anterior | siguiente | | volver |

En la lista de mensajes, mantener 4 durante 2 s borra el mensaje (va a la
papelera; pide confirmación dos veces).

## Backend

El firmware no incluye el servidor. Necesitas uno con esta API (el token va
en la URL como `?token=...`):

| Método y ruta | Qué hace |
|---|---|
| `GET /api/mensajes/nuevos` | Devuelve los mensajes pendientes |
| `POST /api/mensajes/leido` | Cuerpo `{"id": "..."}`: el llavero ya lo tiene, no lo reenvíes |
| `GET /api/audio/:id` | Devuelve el MP3 del mensaje (máx. 4 MB) |
| `DELETE /api/mensajes/:id` | Borra el mensaje del servidor (200 o 404 = borrado) |

Respuesta de `nuevos`:

```json
{
  "nuevos": 1,
  "mensajes": [
    { "id": "abc123", "tipo": "texto", "autor": "Ana", "titulo": "Hola",
      "contenido": "Texto del mensaje", "fecha": "2026-01-01T10:00:00Z" }
  ]
}
```

- `tipo`: `"texto"` o `"audio"` (los de audio no llevan `contenido`).
- `id`: solo `A-Z a-z 0-9 _ -`, 40 caracteres como mucho.
- `fecha`: ISO 8601 en UTC.
- Solo HTTPS. El certificado raíz de confianza está en `src/certs.h`
  (GTS Root R1, el que usa Vercel). Si tu servidor usa otra CA, cámbialo.
- Para que se oigan bien en el altavoz, los audios deberían ir en MP3 mono,
  22 kHz, sin graves por debajo de 200 Hz.

## Fotos

1. Prepara las fotos (necesita ffmpeg):
   ```bash
   tools/fotos/preparar.sh foto1.heic foto2.jpg carpeta/
   ```
   Deja `01.jpg`, `02.jpg`... en `./fotos`, ya a 280x240.
2. Copia la carpeta `fotos` a la raíz de la SD (50 como mucho).
3. Reinicia el llavero.

## Sonido de notificación

Va en `/sonidos/notificacion.mp3` de la SD. Si no hay ninguno, el firmware
copia uno por defecto. Para usar otro:
`tools/sonido/preparar.sh entrada.mp3` y copia el resultado a la SD.

## Estructura del código

- `src/*_rules.h`: lógica pura (sin hardware), con tests en `test/`.
- `src/ui_manager.cpp`, `src/ui_input.cpp`: máquina de estados y botones de la UI.
- `src/tasks.cpp`: tareas FreeRTOS (red, SD, botones, UI, audio).
- `src/api_client.cpp`: cliente HTTPS del backend.
- `src/storage.cpp`: SD, `config.json` y escritura atómica.
- `lib/esp32-audioI2S-patched/`: librería de audio recortada a MP3 y WAV
  (ver su `PARCHE.md`).

## Limitaciones

- No mide la batería.
- Lo que se copie a mano a la SD se ve tras reiniciar.
- En Wokwi la orientación de la pantalla no es correcta (limitación del
  simulador).

## Licencia

GPL-3.0 (ver `LICENSE`). El firmware incluye y enlaza
[ESP32-audioI2S](https://github.com/schreibfaul1/ESP32-audioI2S), que es
GPL-3.0. Las fuentes de `src/fonts/` son de GNU FreeFont (GPL-3.0 con excepción de fuentes).
