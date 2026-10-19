# ESP32-audioI2S parcheada (solo MP3 y WAV)

Copia modificada de [schreibfaul1/ESP32-audioI2S](https://github.com/schreibfaul1/ESP32-audioI2S),
tag **3.4.7**. Licencia GPL-3.0 (ver `LICENSE`); este fichero deja constancia de
los cambios, como pide la licencia.

## Por qué

La librería enlaza todos sus decodificadores aunque solo se use uno y no tiene
opciones para desactivarlos. En la placa (`waveshare_esp32_s3_zero`) ocupaba
2.251.739 B de flash (71,6 %); con este parche, 1.655.179 B (52,6 %): **583 KB menos**.

El parche vive en el repo y no en `.pio/` para que no dependa de descargas ni de
actualizaciones externas.

## Qué se ha cambiado

Carpetas borradas de `src/`:
- `aac_decoder/` (incluida `libfaad/`)
- `flac_decoder/`
- `opus_decoder/`
- `vorbis_decoder/`

Se mantienen `mp3_decoder/` y `wav_decoder/` (este último para posibles sonidos
internos en WAV).

No copiados del original (no hacen falta para compilar): `examples/`,
`additional_info/`, `.github/`.

`src/Audio.cpp`, en 3 puntos marcados con `// PARCHE llavero:`:
1. Includes: solo `mp3_decoder.h` y `wav_decoder.h`.
2. `createDecoder()`: solo crea MP3 y WAV. Para AAC, FLAC, Opus o Vorbis
   devuelve `nullptr` y la librería lo trata como error ("Decoder could not be
   initialized") y para la reproducción.
3. `decodeContinue()`: quitadas las ramas de AAC, FLAC, Opus y Vorbis, que usaban
   constantes de esos decodificadores.

`Audio.h` no se toca: la API pública es la misma. El parseo de cabeceras FLAC,
M4A y OGG que hay dentro de `Audio.cpp` sigue ahí como código muerto; quitarlo
exigiría reescribir buena parte del fichero.

Metadatos: `library.json` y `library.properties` renombrados a
`ESP32-audioI2S-mp3` versión `3.4.7-mp3`, para no confundirla con la original.
No declara dependencias (la original tampoco).

El diff exacto de `Audio.cpp` frente al original está en `parche-audio-cpp.diff`.

## Cómo se usa

PlatformIO compila automáticamente las librerías de `lib/` cuando el código
incluye su cabecera (`#include <Audio.h>` en `src/audio_player.cpp`). Por eso no
aparece en `lib_deps` de `platformio.ini`. Sigue haciendo falta
`-DCORE_DEBUG_LEVEL=1` en `build_flags`.

## Cómo actualizar a otra versión

1. Descargar la versión nueva:
   ```sh
   git clone --depth 1 --branch <tag> https://github.com/schreibfaul1/ESP32-audioI2S.git /tmp/audioI2S
   ```
2. Sustituir la copia (desde la raíz del firmware):
   ```sh
   rm -rf lib/esp32-audioI2S-patched/src
   cp -R /tmp/audioI2S/src /tmp/audioI2S/LICENSE /tmp/audioI2S/README.md \
         /tmp/audioI2S/CMakeLists.txt lib/esp32-audioI2S-patched/
   cd lib/esp32-audioI2S-patched/src
   rm -rf aac_decoder flac_decoder opus_decoder vorbis_decoder
   ```
3. Aplicar el parche (desde `lib/esp32-audioI2S-patched/`):
   ```sh
   patch -p1 < parche-audio-cpp.diff
   ```
   Si falla, repetir a mano los 3 puntos de arriba. Con `grep -n "Decoder::" src/Audio.cpp`
   se ven las referencias a decodificadores que queden.
4. Actualizar `version` en `library.json` y `library.properties`, y este fichero.
5. Compilar `wokwi` y `waveshare_esp32_s3_zero` y regenerar el diff:
   ```sh
   diff -u --label a/src/Audio.cpp --label b/src/Audio.cpp /tmp/audioI2S/src/Audio.cpp src/Audio.cpp > parche-audio-cpp.diff
   ```
6. Comprobar la API de `Audio.h` que usa `src/audio_player.cpp`: `setPinout`,
   `forceMono`, `connecttoFS`, `loop`, `isRunning`, `stopSong`, `setVolume` y
   `audio_info_callback`.
