"use client";
import { useState } from "react";
import { supabaseBrowser } from "@/lib/supabase/browser";
import { addPlaylist, musicUploadUrl } from "../actions";

// Wgrywanie piosenek z telefonu: plik idzie prosto do magazynu, serwer wydaje tylko podpisany adres
export function MusicUpload({ id, author }: { id: string; author: string }) {
  const [busy, setBusy] = useState("");
  const [err, setErr] = useState("");

  async function onSubmit(e: React.FormEvent<HTMLFormElement>) {
    e.preventDefault();
    const fd = new FormData(e.currentTarget);
    const files = (fd.getAll("files") as File[]).filter((f) => f.size > 0);
    if (!files.length) return setErr("Wybierz pliki MP3");
    if (files.some((f) => f.size > 20 * 1024 * 1024)) return setErr("Plik może mieć najwyżej 20 MB");
    setErr("");
    const urls: string[] = [];
    try {
      for (const [i, f] of files.entries()) {
        setBusy(`Wysyłam ${i + 1} z ${files.length}...`);
        const up = await musicUploadUrl(id, f.name);
        const { error } = await supabaseBrowser().storage.from("music").uploadToSignedUrl(up.path, up.token, f, { contentType: "audio/mpeg" });
        if (error) throw error;
        urls.push(up.url);
      }
      setBusy("Zapisuję...");
      await addPlaylist(id, String(fd.get("name") ?? ""), urls, author);
    } catch {
      setErr("Nie udało się wysłać. Sprawdź internet i spróbuj ponownie.");
      setBusy("");
    }
  }

  return (
    <form onSubmit={onSubmit}>
      <label htmlFor="pname">Nazwa na LUMI</label>
      <input id="pname" name="name" type="text" required maxLength={40} placeholder="np. Piosenki od Ani" />
      <label htmlFor="files">Pliki MP3</label>
      <input id="files" name="files" type="file" accept="audio/mpeg,.mp3" multiple />
      {err && <div className="flash err">{err}</div>}
      <button className="btn primary block" style={{ marginTop: 12 }} disabled={!!busy}>{busy || "Wyślij"}</button>
    </form>
  );
}
