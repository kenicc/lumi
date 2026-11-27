#!/usr/bin/env python3
"""Tło ekranu zegara z dowolnego obrazu: kadrowanie do 320x240, przyciemnienie, RGB565 do src/ui/bg_test.c.

Użycie: python3 tools/make_background.py obraz.jpg [jasność 0.0-1.0, domyślnie 0.42]

Przyciemnienie jest robione tu, a nie na urządzeniu: biały tekst zegara musi być czytelny dla
słabszego wzroku, a mieszanie półprzezroczystej warstwy przy każdym odświeżeniu kosztowałoby czas.
Włączenie w firmware: flaga -DLUMI_TEST_BACKGROUND (platformio.ini albo src/BuildConfig.h).
"""
import os
import sys

from PIL import Image, ImageEnhance, ImageFilter

W, H = 320, 240


def main():
    src = sys.argv[1]
    level = float(sys.argv[2]) if len(sys.argv) > 2 else 0.42
    im = Image.open(src).convert("RGB")
    # kadrowanie "cover": wypełnij cały ekran, nadmiar obetnij po równo
    scale = max(W / im.width, H / im.height)
    im = im.resize((round(im.width * scale), round(im.height * scale)), Image.LANCZOS)
    left, top = (im.width - W) // 2, (im.height - H) // 2
    im = im.crop((left, top, left + W, top + H))
    # lekkie rozmycie: drobne detale za literami męczą wzrok bardziej niż jednolita plama
    im = im.filter(ImageFilter.GaussianBlur(1.2))
    im = ImageEnhance.Brightness(im).enhance(level)
    data = bytearray()
    px = im.tobytes()
    for i in range(0, len(px), 3):
        r, g, b = px[i], px[i + 1], px[i + 2]
        v = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)
        data += bytes((v & 0xFF, v >> 8))  # LVGL RGB565: młodszy bajt pierwszy
    out = os.path.join(os.path.dirname(__file__), "..", "src", "ui", "bg_test.c")
    with open(out, "w") as f:
        f.write("// Tło testowe ekranu zegara - wygenerowane przez tools/make_background.py, nie edytuj ręcznie\n")
        f.write('#include "../BuildConfig.h"\n#ifdef LUMI_TEST_BACKGROUND\n#include "lvgl.h"\n\n')
        f.write("static const uint8_t bg_test_map[] = {\n")
        for i in range(0, len(data), 32):
            f.write("  " + ",".join("0x%02x" % x for x in data[i:i + 32]) + ",\n")
        f.write("};\n\nconst lv_image_dsc_t bg_test = {\n")
        f.write("  .header = {.magic = LV_IMAGE_HEADER_MAGIC, .cf = LV_COLOR_FORMAT_RGB565, .flags = 0,\n")
        f.write("             .w = %d, .h = %d, .stride = %d, .reserved_2 = 0},\n" % (W, H, W * 2))
        f.write("  .data_size = sizeof(bg_test_map),\n  .data = bg_test_map,\n};\n#endif\n")
    im.save(os.path.join(os.path.dirname(__file__), "..", "sim", "out", "bg_test_podglad.png"))
    print("zapisano", os.path.normpath(out))


if __name__ == "__main__":
    main()
