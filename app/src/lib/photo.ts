// Zdjęcie kontaktu: kadrowanie na najciekawszy fragment (zwykle twarz), dwa warianty:
// - dla LUMI: 96 x 96, surowe piksele RGB565 (little-endian) - ESP32 nie dekoduje JPEG-a,
//   tylko wczytuje gotowy obraz w formacie ekranu (18 432 bajty);
// - dla aplikacji: JPEG 192 x 192.
import { createHash } from "node:crypto";
import sharp from "sharp";

export const PHOTO_PX = 96;
export const PHOTO_BYTES = PHOTO_PX * PHOTO_PX * 2;

export async function processPhoto(input: Buffer) {
  const base = sharp(input, { failOn: "error" }).rotate(); // obrót z EXIF (zdjęcia z telefonu)
  const rgb = await base
    .clone()
    .resize(PHOTO_PX, PHOTO_PX, { fit: "cover", position: sharp.strategy.attention })
    .removeAlpha()
    .raw()
    .toBuffer();
  const rgb565 = Buffer.alloc(PHOTO_BYTES);
  for (let i = 0, o = 0; i < rgb.length; i += 3, o += 2) {
    const v = ((rgb[i] >> 3) << 11) | ((rgb[i + 1] >> 2) << 5) | (rgb[i + 2] >> 3);
    rgb565.writeUInt16LE(v, o);
  }
  const jpeg = await base
    .clone()
    .resize(192, 192, { fit: "cover", position: sharp.strategy.attention })
    .jpeg({ quality: 82, mozjpeg: true })
    .toBuffer();
  const hash = createHash("sha256").update(rgb565).digest("hex").slice(0, 16);
  return { hash, rgb565, jpeg };
}
