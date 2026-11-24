"use client";
import { useState } from "react";

interface Place { name: string; admin1?: string; latitude: number; longitude: number }

// Wyszukiwarka miejscowości (Open-Meteo, bez klucza) - LUMI dostaje współrzędne
export function PlaceSearch({ place, lat, lon }: { place: string; lat: number; lon: number }) {
  const [q, setQ] = useState("");
  const [results, setResults] = useState<Place[]>([]);
  const [chosen, setChosen] = useState({ place, lat, lon });
  const [busy, setBusy] = useState(false);

  async function search() {
    if (q.trim().length < 2) return;
    setBusy(true);
    const r = await fetch(`https://geocoding-api.open-meteo.com/v1/search?name=${encodeURIComponent(q)}&count=6&language=pl&countryCode=PL`)
      .then((x) => x.json())
      .catch(() => ({}));
    setResults(r.results ?? []);
    setBusy(false);
  }

  return (
    <>
      <p>Teraz: <b>{chosen.place || "nie wybrano"}</b></p>
      <label htmlFor="q">Szukaj miejscowości</label>
      <div className="row">
        <input id="q" type="text" className="grow" value={q} onChange={(e) => setQ(e.target.value)}
          onKeyDown={(e) => { if (e.key === "Enter") { e.preventDefault(); search(); } }} />
        <button type="button" className="btn" onClick={search} disabled={busy}>Szukaj</button>
      </div>
      <ul className="list">
        {results.map((p) => (
          <li key={`${p.latitude},${p.longitude}`}>
            <label className="check" style={{ padding: "8px 0" }}>
              <input type="radio" name="pick" onChange={() => setChosen({ place: p.name, lat: p.latitude, lon: p.longitude })} />
              <span>{p.name}{p.admin1 ? <span className="dim">, {p.admin1}</span> : null}</span>
            </label>
          </li>
        ))}
      </ul>
      <input type="hidden" name="place" value={chosen.place} />
      <input type="hidden" name="lat" value={chosen.lat} />
      <input type="hidden" name="lon" value={chosen.lon} />
    </>
  );
}
