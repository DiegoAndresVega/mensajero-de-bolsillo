#!/usr/bin/env bash
# Prepara el sonido de notificacion del llavero (/sonidos/notificacion.mp3).
#
# Uso:
#   tools/sonido/preparar.sh                 genera el sonido por defecto (dos notas)
#                                            en assets/sonidos/notificacion.mp3
#   tools/sonido/preparar.sh entrada.mp3     convierte un sonido descargado y deja
#                                            notificacion.mp3 en la carpeta actual
#
# Lo deja en el formato que mejor suena en el altavoz: mono, 22 kHz,
# 64 kbps CBR, sin graves por debajo de 200 Hz y con el pico a -3 dBFS.
# Ademas lo corta a MAX_SECONDS con un fundido al final. Necesita ffmpeg.
set -euo pipefail

readonly RATE=22050
readonly KBPS=64
readonly TARGET_PEAK_DB=-3
readonly HIGHPASS_HZ=200
# Un aviso largo molesta y tapa el mensaje de voz que interrumpe
readonly MAX_SECONDS=2
readonly FADE_SECONDS=0.3
readonly ROOT_DIR="$(cd "$(dirname "$0")/../.." && pwd)"

if ! command -v ffmpeg >/dev/null; then
  echo "Falta ffmpeg (brew install ffmpeg)" >&2
  exit 1
fi

TMP_DIR="$(mktemp -d)"
trap 'rm -rf "$TMP_DIR"' EXIT

ff() { ffmpeg -hide_banner -loglevel error -y "$@"; }

peak_db() {
  ffmpeg -hide_banner -nostats -i "$1" -af volumedetect -f null - 2>&1 |
    sed -n 's/.*max_volume: \(-*[0-9.]*\) dB.*/\1/p'
}

if [[ $# -eq 0 ]]; then
  # Dos notas de campanita (do6 y sol6) que se apagan solas
  readonly NOTE1="sin(2*PI*1046.5*t)*exp(-7*t)"
  readonly NOTE2="if(gte(t,0.16),sin(2*PI*1568*(t-0.16))*exp(-5*(t-0.16)),0)"
  ff -f lavfi -i "aevalsrc='0.4*($NOTE1+$NOTE2)':s=$RATE:d=1" "$TMP_DIR/in.wav"
  readonly INPUT="$TMP_DIR/in.wav"
  readonly OUTPUT="$ROOT_DIR/assets/sonidos/notificacion.mp3"
else
  readonly INPUT="$1"
  readonly OUTPUT="$PWD/notificacion.mp3"
fi
if [[ ! -f "$INPUT" ]]; then
  echo "No existe: $INPUT" >&2
  exit 1
fi
mkdir -p "$(dirname "$OUTPUT")"

fade_start=$(echo "$MAX_SECONDS - $FADE_SECONDS" | bc)
readonly SHAPE="aformat=channel_layouts=mono,atrim=0:$MAX_SECONDS,afade=t=out:st=$fade_start:d=$FADE_SECONDS,highpass=f=$HIGHPASS_HZ:poles=2,highpass=f=$HIGHPASS_HZ:poles=2"
encode() { # filtro salida
  ff -i "$INPUT" -af "$1" -ar $RATE -c:a libmp3lame -b:a ${KBPS}k "$2"
}

# Dos pasadas: el pico se mide en el MP3 final (codificar sube algo los picos)
encode "$SHAPE" "$TMP_DIR/pass1.mp3"
gain=$(echo "$TARGET_PEAK_DB - $(peak_db "$TMP_DIR/pass1.mp3")" | bc)
encode "$SHAPE,volume=${gain}dB" "$OUTPUT"

echo "Listo: $OUTPUT ($(wc -c <"$OUTPUT" | tr -d ' ') bytes, pico $(peak_db "$OUTPUT") dB)"
echo "Copialo a la SD como /sonidos/notificacion.mp3"
