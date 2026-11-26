import { saveAccess } from "../actions";
import { requireMember } from "@/lib/access";
import { currentSettings } from "@/lib/store";
import { Flash, SaveBar, Top, type SP } from "@/components/ui";

const TREMOR = [
  { v: 0, t: "Brak" },
  { v: 200, t: "Lekkie drżenie dłoni" },
  { v: 450, t: "Średnie" },
  { v: 800, t: "Silne" },
];

export default async function Access({ params, searchParams }: { params: Promise<{ id: string }>; searchParams: SP }) {
  const { id } = await params;
  const { ok, err } = await searchParams;
  const { device } = await requireMember(id, "access");
  const s = currentSettings(device.settings);
  const a = s.access;
  return (
    <main>
      <Top back={{ href: `/d/${id}`, label: device.name ?? id }} />
      <h1>Dostępność</h1>
      <p className="dim">Te ustawienia zmienia tylko główny opiekun.</p>
      <Flash ok={ok} err={err} />
      <form action={saveAccess.bind(null, id)}>
        <section className="card">
          <h2 style={{ marginTop: 0 }}>Widoczne funkcje</h2>
          <label className="check"><input type="checkbox" name="fCalls" defaultChecked={s.features.calls} /> Rozmowy (przycisk Dzwoń)</label>
          <label className="check"><input type="checkbox" name="fMedia" defaultChecked={s.features.media} /> Radio i muzyka</label>
          <label className="check"><input type="checkbox" name="fWeather" defaultChecked={s.features.weather} /> Pogoda</label>
        </section>
        <section className="card">
          <h2 style={{ marginTop: 0 }}>Rozmowy</h2>
          <label className="check">
            <input type="checkbox" name="onlyContacts" defaultChecked={s.calls.onlyContacts} />
            <span>Odbieraj tylko od osób z listy kontaktów<br /><span className="dim small">Chroni przed oszustami „na wnuczka” i telemarketingiem.</span></span>
          </label>
          <label htmlFor="autoAnswerSec">Automatyczne odbieranie po</label>
          <select id="autoAnswerSec" name="autoAnswerSec" defaultValue={a.autoAnswerSec}>
            {[3, 5, 7, 10].map((v) => <option key={v} value={v}>{v} sekundach</option>)}
          </select>
          <p className="dim small">Działa tylko dla osób zaznaczonych w Kontaktach.</p>
        </section>
        <section className="card">
          <h2 style={{ marginTop: 0 }}>Przyciski</h2>
          <label htmlFor="minPressMs">Drżenie dłoni</label>
          <select id="minPressMs" name="minPressMs" defaultValue={a.minPressMs}>
            {TREMOR.map((t) => <option key={t.v} value={t.v}>{t.t}</option>)}
          </select>
          <p className="dim small">Przy drżeniu LUMI ignoruje bardzo krótkie dotknięcia.</p>
          <label htmlFor="repeatIgnoreMs">Ignoruj powtórzone naciśnięcia przez</label>
          <select id="repeatIgnoreMs" name="repeatIgnoreMs" defaultValue={a.repeatIgnoreMs}>
            {[250, 500, 1000, 1500].map((v) => <option key={v} value={v}>{v / 1000} s</option>)}
          </select>
          <label className="check">
            <input type="checkbox" name="oneButton" defaultChecked={a.oneButton} />
            <span>Tryb jednego przycisku<br /><span className="dim small">Środkowy przycisk przechodzi po opcjach, przytrzymanie wybiera. Dla ograniczonej sprawności rąk.</span></span>
          </label>
          <label className="check"><input type="checkbox" name="lockMusic" defaultChecked={a.locked.includes("music")} /> Zablokuj przycisk Muzyka</label>
          <label className="check"><input type="checkbox" name="lockCall" defaultChecked={a.locked.includes("call")} /> Zablokuj przycisk Dzwoń</label>
          <label className="check">
            <input type="checkbox" name="blockPowerOff" defaultChecked={!a.allowPowerOff} />
            <span>Nie pozwalaj wyłączyć LUMI włącznikiem<br /><span className="dim small">Gdy zdarza się wyłączać je przypadkiem - wyłączone LUMI nie odbierze rozmowy.</span></span>
          </label>
        </section>
        <SaveBar />
      </form>
    </main>
  );
}
