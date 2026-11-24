import { addMedia, deleteMedia, moveMedia } from "../actions";
import { MusicUpload } from "./MusicUpload";
import { getProfile, requireMember } from "@/lib/access";
import { currentSettings } from "@/lib/store";
import { Flash, Top, type SP } from "@/components/ui";

const KIND = { fm: "Radio FM", web: "Radio z internetu", playlist: "Muzyka" } as const;

export default async function Radio({ params, searchParams }: { params: Promise<{ id: string }>; searchParams: SP }) {
  const { id } = await params;
  const { ok, err } = await searchParams;
  const { user, device } = await requireMember(id, "media");
  const s = currentSettings(device.settings);
  const profile = await getProfile(user.id);
  const noAudio = device.variant === "allegro";

  return (
    <main>
      <Top back={{ href: `/d/${id}`, label: device.name ?? id }} />
      <h1>Radio i muzyka</h1>
      <p className="dim">Ta kolejność jest pod przyciskiem Muzyka. Nazwy stacji ustalasz tu - LUMI ich nie zgaduje.</p>
      {noAudio && <p className="flash err">Ten prototyp LUMI nie ma jeszcze modułu radia - lista zadziała po jego dodaniu.</p>}
      <Flash ok={ok} err={err} />
      <ul className="list card">
        {s.media.length === 0 && <li className="dim" style={{ padding: 8 }}>Lista jest pusta.</li>}
        {s.media.map((m, i) => (
          <li key={i} className="row" style={{ padding: "10px 0" }}>
            <div className="grow">
              <b>{m.name}</b>
              <div className="dim small">
                {KIND[m.type]}
                {m.type === "fm" && ` ${m.freq.toFixed(1).replace(".", ",")} MHz`}
                {m.type === "playlist" && ` · ${m.urls.length} plików${m.from ? ` · od: ${m.from}` : ""}`}
              </div>
            </div>
            <form action={moveMedia.bind(null, id, i, -1)}><button className="btn small" disabled={i === 0} aria-label="Wyżej">↑</button></form>
            <form action={moveMedia.bind(null, id, i, 1)}><button className="btn small" disabled={i === s.media.length - 1} aria-label="Niżej">↓</button></form>
            <form action={deleteMedia.bind(null, id, i)}><button className="btn small danger" aria-label={`Usuń ${m.name}`}>✕</button></form>
          </li>
        ))}
      </ul>
      <details className="card">
        <summary>Dodaj stację FM</summary>
        <form action={addMedia.bind(null, id)}>
          <input type="hidden" name="type" value="fm" />
          <label htmlFor="fmname">Nazwa</label>
          <input id="fmname" name="name" type="text" required maxLength={40} placeholder="np. Jedynka" />
          <label htmlFor="freq">Częstotliwość (MHz)</label>
          <input id="freq" name="freq" type="text" inputMode="decimal" required placeholder="99,4" />
          <button className="btn primary block" style={{ marginTop: 12 }}>Dodaj</button>
        </form>
      </details>
      <details className="card">
        <summary>Dodaj radio internetowe</summary>
        <form action={addMedia.bind(null, id)}>
          <input type="hidden" name="type" value="web" />
          <label htmlFor="wname">Nazwa</label>
          <input id="wname" name="name" type="text" required maxLength={40} />
          <label htmlFor="url">Adres strumienia (MP3 lub AAC)</label>
          <input id="url" name="url" type="url" required placeholder="http://..." />
          <p className="dim small">Radio internetowe gra przez Wi-Fi.</p>
          <button className="btn primary block" style={{ marginTop: 12 }}>Dodaj</button>
        </form>
      </details>
      <details className="card">
        <summary>Wyślij piosenki z telefonu</summary>
        <MusicUpload id={id} author={profile?.display_name ?? ""} />
      </details>
    </main>
  );
}
