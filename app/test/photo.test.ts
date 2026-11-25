import { describe, expect, it } from "vitest";
import sharp from "sharp";
import { PHOTO_BYTES, processPhoto } from "@/lib/photo";

describe("processPhoto", () => {
  it("daje 96x96 RGB565 i JPEG, a kolory są w formacie ekranu", async () => {
    // czerwony obraz 400x300 jak z telefonu
    const red = await sharp({ create: { width: 400, height: 300, channels: 3, background: { r: 255, g: 0, b: 0 } } }).png().toBuffer();
    const p = await processPhoto(red);
    expect(p.rgb565.length).toBe(PHOTO_BYTES);
    expect(p.rgb565.readUInt16LE(0)).toBe(0xf800); // czysta czerwień w RGB565
    expect(p.hash).toMatch(/^[0-9a-f]{16}$/);
    const meta = await sharp(p.jpeg).metadata();
    expect([meta.width, meta.height, meta.format]).toEqual([192, 192, "jpeg"]);
  });
  it("odrzuca coś, co nie jest obrazem", async () => {
    await expect(processPhoto(Buffer.from("to nie jest zdjęcie"))).rejects.toThrow();
  });
});
