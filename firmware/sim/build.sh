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
$OBJ/lumi_sim
python3 sim/render.py
