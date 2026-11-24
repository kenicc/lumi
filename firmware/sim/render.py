"""Zamienia zrzuty PPM na PNG i wkleja je w uproszczony obrys obudowy LUMI (105 x 80 mm),
żeby było widać, czy etykiety stoją przy przyciskach."""
import glob
import os

from PIL import Image, ImageDraw

MM = 6  # px na mm
OUT = os.path.join(os.path.dirname(__file__), "out")


def device(screen: Image.Image) -> Image.Image:
    w, h = 105 * MM, 80 * MM
    img = Image.new("RGB", (w + 40, h + 40), (236, 229, 216))
    d = ImageDraw.Draw(img)
    ox, oy = 20, 20
    d.rounded_rectangle([ox, oy, ox + w, oy + h], radius=8 * MM, fill=(40, 42, 46))
    # aktywny obszar ekranu 57,6 x 43,2 mm; środek ekranu na wysokości środkowego przycisku
    sw, sh = int(57.6 * MM), int(43.2 * MM)
    col_x = 92 * MM  # środek prawej kolumny przycisków
    mid_y = 30 * MM  # środek środkowego przycisku
    sx = ox + 6 * MM
    sy = oy + mid_y - sh // 2
    d.rounded_rectangle([sx - 2 * MM, sy - 2 * MM, sx + sw + 2 * MM, sy + sh + 2 * MM], radius=2 * MM, fill=(10, 10, 12))
    img.paste(screen.resize((sw, sh), Image.LANCZOS), (sx, sy))
    r = 8 * MM
    for i in (-1, 0, 1):
        cy = oy + mid_y + i * 18 * MM
        d.ellipse([ox + col_x - r, cy - r, ox + col_x + r, cy + r], fill=(30, 32, 35), outline=(70, 72, 76), width=2)
        if i == -1:
            d.ellipse([ox + col_x - 5, cy - 5, ox + col_x + 5, cy + 5], fill=(110, 112, 116))
        if i == 1:
            d.rounded_rectangle([ox + col_x - 12, cy - 3, ox + col_x + 12, cy + 3], radius=3, fill=(110, 112, 116))
    for j, label in enumerate(("nuta", "dom", "słuchawka")):
        cx = ox + (sx - ox) + sw // 2 + (j - 1) * 25 * MM
        cy = oy + 68 * MM
        ring = (76, 195, 138) if j == 2 else (70, 72, 76)
        d.ellipse([cx - r, cy - r, cx + r, cy + r], fill=(30, 32, 35), outline=ring, width=3 if j == 2 else 2)
    return img


for p in sorted(glob.glob(os.path.join(OUT, "*.ppm"))):
    im = Image.open(p).convert("RGB")
    base = p[:-4]
    im.resize((640, 480), Image.NEAREST).save(base + ".png")
    device(im).save(base + "_obudowa.png")
    os.remove(p)
print("PNG w", OUT)
