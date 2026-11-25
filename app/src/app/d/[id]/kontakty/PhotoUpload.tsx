"use client";
import { useState } from "react";
import { uploadContactPhoto } from "../actions";

// Zdjęcie z telefonu jest zmniejszane w przeglądarce (ok. 100 KB zamiast kilku MB) przed wysłaniem.
async function shrink(file: File): Promise<Blob> {
  const bmp = await createImageBitmap(file, { imageOrientation: "from-image" });
  const scale = Math.min(1, 720 / Math.max(bmp.width, bmp.height));
  const c = document.createElement("canvas");
  c.width = Math.round(bmp.width * scale);
  c.height = Math.round(bmp.height * scale);
  c.getContext("2d")!.drawImage(bmp, 0, 0, c.width, c.height);
  return new Promise((res, rej) => c.toBlob((b) => (b ? res(b) : rej(new Error("canvas"))), "image/jpeg", 0.85));
}

export function PhotoUpload({ id, cid }: { id: string; cid: string }) {
  const [busy, setBusy] = useState(false);
  const [err, setErr] = useState("");
  async function onChange(e: React.ChangeEvent<HTMLInputElement>) {
    const f = e.target.files?.[0];
    if (!f) return;
    setBusy(true);
    setErr("");
    try {
      const fd = new FormData();
      fd.set("photo", new File([await shrink(f)], "zdjecie.jpg", { type: "image/jpeg" }));
      await uploadContactPhoto(id, cid, fd);
    } catch (x) {
      // przekierowanie po zapisie też przychodzi jako wyjątek - tylko prawdziwe błędy pokazujemy
      if (!String(x).includes("NEXT_REDIRECT")) setErr("Nie udało się wysłać zdjęcia. Spróbuj ponownie.");
      setBusy(false);
    }
  }
  return (
    <>
      <label className="btn block" style={{ fontWeight: 700, margin: 0 }}>
        {busy ? "Wysyłam..." : "Wybierz lub zrób zdjęcie"}
        <input type="file" accept="image/*" onChange={onChange} disabled={busy} style={{ display: "none" }} />
      </label>
      <p className="dim small">LUMI sam wykadruje twarz. Najlepiej wyraźne zdjęcie z bliska, na jasnym tle.</p>
      {err && <div className="flash err">{err}</div>}
    </>
  );
}
