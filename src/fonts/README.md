# Fuentes con Latin-1

`FreeSans9pt8b.h`, `FreeSansBold9pt8b.h` y `FreeSansBold12pt8b.h`: GNU FreeFont
(FreeSans y FreeSansBold, versión 20120503) convertidas a formato GFX con
`fontconvert` de Adafruit-GFX-Library, caracteres 0x20-0xFF. Los ficheros son la
salida sin tocar.

Licencia: GPLv3 con la excepción de fuentes de GNU FreeFont (incluir la fuente
en un documento o programa no hace que ese programa quede bajo la GPL).

## Regenerarlas (macOS)

```sh
brew install freetype
curl -LO https://raw.githubusercontent.com/adafruit/Adafruit-GFX-Library/master/fontconvert/fontconvert.c
curl -LO https://raw.githubusercontent.com/adafruit/Adafruit-GFX-Library/master/gfxfont.h
sed -i '' 's|"../gfxfont.h"|"gfxfont.h"|' fontconvert.c
cc -O2 -o fontconvert fontconvert.c $(freetype-config --cflags) $(freetype-config --libs)
curl -LO https://ftp.gnu.org/gnu/freefont/freefont-ttf-20120503.zip && unzip freefont-ttf-20120503.zip
./fontconvert freefont-20120503/FreeSans.ttf 9 32 255 > FreeSans9pt8b.h
./fontconvert freefont-20120503/FreeSansBold.ttf 9 32 255 > FreeSansBold9pt8b.h
./fontconvert freefont-20120503/FreeSansBold.ttf 12 32 255 > FreeSansBold12pt8b.h
```

Otro tamaño: la misma orden con otro número de puntos, añadir el `.h` a
`fonts.cpp` y su `extern` a `fonts.h`.
