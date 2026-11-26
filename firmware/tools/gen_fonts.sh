#!/usr/bin/env bash
# Generuje czcionki LVGL z polskimi znakami i ikonami.
# Krój: Atkinson Hyperlegible (Braille Institute) - zaprojektowany dla osób słabowidzących,
# wyraźnie odróżnia I/l/1, O/0, b/d. Ikony: FontAwesome 5 z paczki LVGL.
# Użycie: tools/gen_fonts.sh <katalog_node_modules> <plik_FontAwesome.woff>
#   npm i lv_font_conv @fontsource/atkinson-hyperlegible
set -euo pipefail
NM=${1:?node_modules}
FA=${2:?FontAwesome5 woff}
OUT=$(dirname "$0")/../src/ui/fonts
F=$NM/@fontsource/atkinson-hyperlegible/files
CONV="node $NM/lv_font_conv/lv_font_conv.js"
LAT="0x20-0x7E,0xB0,0xB7,0xD3,0xF3"
PL="0x104-0x107,0x118,0x119,0x141-0x144,0x15A,0x15B,0x179-0x17C"
# ikony - musi się zgadzać z src/ui/Icons.h
ICONS="0xF095,0xF3DD,0xF001,0xF015,0xF04B,0xF04C,0xF077,0xF078,0xF00C,0xF00D,0xF028,0xF0F3,0xF1F6,0xF240,0xF241,0xF242,0xF243,0xF244,0xF0E7,0xF1EB,0xF012,0xF185,0xF0C2,0xF6C4,0xF73D,0xF740,0xF2DC,0xF75F,0xF519,0xF0AC,0xF007,0xF011,0xF071,0xF05A,0xF060,0xF074"

gen() { # nazwa rozmiar waga zakres_ikon
  local name=$1 size=$2 w=$3 icons=$4
  local args=(--bpp 4 --size "$size" --no-compress --format lvgl --lv-include lvgl.h -o "$OUT/$name.c"
    --font "$F/atkinson-hyperlegible-latin-$w-normal.woff" -r "$LAT"
    --font "$F/atkinson-hyperlegible-latin-ext-$w-normal.woff" -r "$PL")
  if [ -n "$icons" ]; then args+=(--font "$FA" -r "$icons"); fi
  $CONV "${args[@]}"
}
gen_digits() { # duże cyfry zegara i temperatury
  local name=$1 size=$2
  $CONV --bpp 4 --size "$size" --no-compress --format lvgl --lv-include lvgl.h -o "$OUT/$name.c" \
    --font "$F/atkinson-hyperlegible-latin-700-normal.woff" -r "0x20,0x2D,0x30-0x3A,0xB0"
}
for s in 20 24 28 36 48; do gen "lumi_b$s" $s 700 "$ICONS"; done
for s in 20 24 28; do gen "lumi_r$s" $s 400 ""; done
gen_digits lumi_d72 72
gen_digits lumi_d96 96
echo OK
