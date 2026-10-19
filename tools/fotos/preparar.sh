#!/usr/bin/env bash
# Prepara fotos para el llavero (/fotos en la SD).
#
# Uso:
#   tools/fotos/preparar.sh foto1.heic foto2.jpg carpeta_con_fotos/ ...
#
# Deja en ./fotos (carpeta actual) 01.jpg, 02.jpg... en el orden dado (las de
# una carpeta, por nombre). Esa carpeta se copia tal cual a la raiz de la SD.
#
# Cada foto sale como la quiere el llavero: JPEG baseline de 280x240, con la
# rotacion del movil ya aplicada. Las verticales se giran 90 grados para que
# llenen la pantalla (se ven girando el llavero). Lo que sobra se recorta por
# el centro. Acepta JPG, PNG y HEIC. Necesita ffmpeg (y sips, que trae macOS,
# para HEIC).
set -euo pipefail

readonly WIDTH=280
readonly HEIGHT=240
# Calidad JPEG de ffmpeg: 2 (mejor) a 31 (peor). 3 deja fotos de 20-40 KB.
readonly QUALITY=3
# Giro de las verticales: 2 = antihorario (lo de arriba de la foto queda a
# la izquierda; se ve girando el llavero en sentido horario), 1 = horario
readonly PORTRAIT_TRANSPOSE=2
# Lo mismo que PHOTOS_MAX en src/config.h
readonly MAX_PHOTOS=50
readonly OUT_DIR="$PWD/fotos"

if ! command -v ffmpeg >/dev/null; then
  echo "Falta ffmpeg (brew install ffmpeg)" >&2
  exit 1
fi
if [[ $# -eq 0 ]]; then
  sed -n '2,14p' "$0" | sed 's/^# \{0,1\}//'
  exit 1
fi

TMP_DIR="$(mktemp -d)"
trap 'rm -rf "$TMP_DIR"' EXIT

ff() { ffmpeg -hide_banner -loglevel error -y "$@"; }

is_image() {
  local ext
  ext="$(echo "${1##*.}" | tr '[:upper:]' '[:lower:]')"
  [[ "$ext" == jpg || "$ext" == jpeg || "$ext" == png || "$ext" == heic || "$ext" == heif ]]
}

# Lista de entrada: ficheros sueltos y el contenido de las carpetas, por nombre
inputs=()
for arg in "$@"; do
  if [[ -d "$arg" ]]; then
    while IFS= read -r f; do
      is_image "$f" && inputs+=("$f")
    done < <(find "$arg" -maxdepth 1 -type f ! -name '.*' | sort)
  elif [[ -f "$arg" ]] && is_image "$arg"; then
    inputs+=("$arg")
  else
    echo "Se salta (no es una foto): $arg" >&2
  fi
done
if [[ ${#inputs[@]} -eq 0 ]]; then
  echo "No hay fotos que preparar" >&2
  exit 1
fi
if [[ ${#inputs[@]} -gt $MAX_PHOTOS ]]; then
  echo "Hay ${#inputs[@]} fotos; el llavero solo muestra $MAX_PHOTOS. Se preparan las primeras." >&2
  inputs=("${inputs[@]:0:$MAX_PHOTOS}")
fi

if [[ -d "$OUT_DIR" ]] && [[ -n "$(ls -A "$OUT_DIR")" ]]; then
  echo "Ya existe $OUT_DIR con ficheros. Borrala o muevete a otra carpeta." >&2
  exit 1
fi
mkdir -p "$OUT_DIR"

n=0
for input in "${inputs[@]}"; do
  n=$((n + 1))
  name="$(printf '%02d.jpg' "$n")"
  source="$input"
  ext="$(echo "${input##*.}" | tr '[:upper:]' '[:lower:]')"
  if [[ "$ext" == heic || "$ext" == heif ]]; then
    sips -s format jpeg "$input" --out "$TMP_DIR/heic.jpg" >/dev/null
    source="$TMP_DIR/heic.jpg"
  fi
  # Primera pasada: ffmpeg aplica la rotacion EXIF del movil. Asi se sabe si
  # la foto, tal como se ve, es vertical.
  ff -i "$source" -frames:v 1 "$TMP_DIR/upright.png"
  w="$(sips -g pixelWidth "$TMP_DIR/upright.png" | awk '/pixelWidth/ {print $2}')"
  h="$(sips -g pixelHeight "$TMP_DIR/upright.png" | awk '/pixelHeight/ {print $2}')"
  rotate=""
  if [[ "$h" -gt "$w" ]]; then
    rotate="transpose=$PORTRAIT_TRANSPOSE,"
  fi
  # El codificador mjpeg de ffmpeg siempre hace JPEG baseline (no progresivo)
  ff -i "$TMP_DIR/upright.png" \
    -vf "${rotate}scale=$WIDTH:$HEIGHT:force_original_aspect_ratio=increase,crop=$WIDTH:$HEIGHT" \
    -q:v $QUALITY -pix_fmt yuvj420p -frames:v 1 "$OUT_DIR/$name"
  echo "$name <- $(basename "$input")${rotate:+ (vertical, girada)}"
done

echo "Listo: $n fotos en $OUT_DIR"
echo "Copia la carpeta 'fotos' a la raiz de la SD (debe quedar /fotos/01.jpg...)."
