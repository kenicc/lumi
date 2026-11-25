#!/usr/bin/env bash
# Buduje symulator ekranu i robi zrzuty do sim/out/*.png
# Wymaga: g++, python3 + Pillow, biblioteki LVGL pobranej przez PlatformIO (pio pkg install -e proto)
set -euo pipefail
cd "$(dirname "$0")/.."
LV=.pio/libdeps/proto/lvgl
AJ=.pio/libdeps/proto/ArduinoJson/src
OBJ=sim/obj
mkdir -p $OBJ sim/out
INC="-Isim/stubs -Iinclude -Isrc -I$LV -I$AJ -DLV_CONF_INCLUDE_SIMPLE -DLV_LVGL_H_INCLUDE_SIMPLE"
# LVGL kompilujemy raz
if [ ! -f $OBJ/liblvgl.a ]; then
  find $LV/src -name '*.c' | while read -r f; do
    o=$OBJ/lv_$(echo "$f" | md5sum | cut -c1-12).o
    gcc -O1 -w $INC -c "$f" -o "$o"
  done
  ar rcs $OBJ/liblvgl.a $OBJ/lv_*.o
fi
for f in src/ui/fonts/*.c; do gcc -O1 -w $INC -c "$f" -o "$OBJ/$(basename "$f" .c).o"; done
g++ -std=gnu++17 -O1 -g $INC sim/sim.cpp src/app/App.cpp src/ui/Ui.cpp src/core/*.cpp $OBJ/lumi_*.o $OBJ/liblvgl.a -o $OBJ/lumi_sim
# przykładowe zdjęcie kontaktu: ilustracja przepuszczona przez potok serwera (app/src/lib/photo.ts)
mkdir -p sim/out/cfg/ph
if [ -d ../app/node_modules/sharp ]; then
  (cd ../app && node --experimental-strip-types -e '
    const sharp = require("sharp");
    const svg = `<svg xmlns="http://www.w3.org/2000/svg" width="400" height="500"><rect width="400" height="500" fill="#8fb8d8"/>
      <ellipse cx="200" cy="520" rx="170" ry="150" fill="#c0504d"/><rect x="170" y="300" width="60" height="70" fill="#e8b48f"/>
      <ellipse cx="200" cy="230" rx="95" ry="115" fill="#f0c4a0"/><path d="M105 210 Q110 95 200 100 Q290 95 295 210 Q280 150 200 150 Q120 150 105 210Z" fill="#6b4a2f"/>
      <circle cx="165" cy="230" r="9" fill="#333"/><circle cx="235" cy="230" r="9" fill="#333"/><path d="M165 290 Q200 315 235 290" stroke="#a0453a" stroke-width="7" fill="none"/></svg>`;
    sharp(Buffer.from(svg)).resize(96, 96, { fit: "cover", position: sharp.strategy.attention }).removeAlpha().raw().toBuffer().then((rgb) => {
      const out = Buffer.alloc(96 * 96 * 2);
      for (let i = 0, o = 0; i < rgb.length; i += 3, o += 2) out.writeUInt16LE(((rgb[i] >> 3) << 11) | ((rgb[i + 1] >> 2) << 5) | (rgb[i + 2] >> 3), o);
      require("fs").writeFileSync("../firmware/sim/out/cfg/ph/0123456789abcdef.565", out);
    });')
fi
$OBJ/lumi_sim
python3 sim/render.py
