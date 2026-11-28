import { saveHome } from "../actions";
import { requireMember } from "@/lib/access";
import { currentSettings } from "@/lib/store";
import { Flash, SaveBar, Top, type SP } from "@/components/ui";

const POS = ["Prawy górny", "Prawy środkowy", "Prawy dolny"];

export default async function HomeScreen({ params, searchParams }: { params: Promise<{ id: string }>; searchParams: SP }) {
  const { id } = await params;
  const { ok, err } = await searchParams;
  const { device } = await requireMember(id, "home");
  const s = currentSettings(device.settings);
  const options = [
    { v: "", t: "(pusty)" },
    ...s.contacts.map((c) => ({ v: `contact:${c.id}`, t: `Zadzwoń: ${c.name}` })),
    { v: "media", t: "Radio i muzyka" },
    { v: "weather", t: "Pogoda na jutro" },
  ];

  const wx = s.features.weather;
  return (
    <main>
      <Top back={{ href: `/d/${id}`, label: device.name ?? id }} />
      <h1>Ekran główny</h1>
      <p className="dim">
        Na zegarze jest tylko to, co najważniejsze: duża godzina, dzień i data oraz pogoda. Bez przycisków na ekranie -
        senior nie musi niczego czytać, żeby zadzwonić.
      </p>
      <Flash ok={ok} err={err} />
      {/* podgląd: tak to wygląda na LUMI */}
      <div className="card" style={{ background: "#0b0b0d", color: "#f4f1ea", border: 0, padding: 10, display: "grid", gap: 6 }}>
        <div style={{ background: "#1c1d21", borderRadius: 14, padding: "10px 12px", textAlign: "center" }}>
          <div style={{ fontSize: 64, fontWeight: 600, lineHeight: 1, letterSpacing: "-0.02em", fontVariantNumeric: "tabular-nums" }}>10:24</div>
          <div style={{ display: "flex", alignItems: "center", gap: 8, margin: "8px auto 0", width: "86%" }}>
            <div style={{ flex: 1, height: 10, borderRadius: 99, background: "#2a2b30", overflow: "hidden" }}>
              <div style={{ width: "76%", height: "100%", background: "#f4f1ea" }} />
            </div>
            <span style={{ fontSize: 13 }}>76%</span>
          </div>
        </div>
        <div style={{ display: "grid", gridTemplateColumns: wx ? "1fr auto" : "1fr", gap: 6 }}>
          <div style={{ background: "#1c1d21", borderRadius: 14, padding: "8px 12px" }}>
            <div style={{ fontSize: 22, fontWeight: 700 }}>Wtorek{s.home.partOfDay ? <span style={{ color: "#f2b04b", fontWeight: 600 }}> rano</span> : null}</div>
            <div style={{ color: "#a39e94", fontWeight: 600 }}>24 listopada</div>
          </div>
          {wx && (
            <div style={{ background: "#232a33", borderRadius: 14, padding: "8px 14px", display: "flex", alignItems: "center", gap: 6, fontSize: 30, fontWeight: 700 }}>
              <span aria-hidden>☁</span>6°
            </div>
          )}
        </div>
      </div>
      <form action={saveHome.bind(null, id)} className="card">
        <label className="check">
          <input type="checkbox" name="partOfDay" defaultChecked={s.home.partOfDay} />
          <span>Pokazuj porę dnia („Wtorek rano”)<br /><span className="dim small">Pomaga w orientacji, gdy dni się mylą. Pojawia się, gdy mieści się obok dnia tygodnia.</span></span>
        </label>
        <details>
          <summary>Skróty w trybie jednego przycisku</summary>
          <p className="dim small">
            Tylko gdy w Dostępności włączony jest tryb jednego przycisku: wtedy przy zegarze są trzy skróty, a szybkie wybieranie
            zawsze pyta „Zadzwonić?”.
          </p>
          {POS.map((name, i) => (
            <div key={i}>
              <label htmlFor={`slot${i}`}>{name} przycisk</label>
              <select id={`slot${i}`} name={`slot${i}`} defaultValue={s.home.slots[i]}>
                {options.map((o) => <option key={o.v} value={o.v}>{o.t}</option>)}
              </select>
            </div>
          ))}
        </details>
        <SaveBar />
      </form>
      <div className="card">
        <h2 style={{ marginTop: 0 }}>Przyciski przy zegarze</h2>
        <ul className="list">
          <li style={{ padding: "8px 0" }}><b>Środkowy boczny</b> - kontakty (dzwonienie)</li>
          {wx && <li style={{ padding: "8px 0" }}><b>Dolny boczny</b> - pogoda na dziś i jutro</li>}
          <li style={{ padding: "8px 0" }}><b>Dzwoń</b> (zielony) - kontakty, odbieranie rozmów</li>
          <li style={{ padding: "8px 0" }}><b>Muzyka</b> - radio, muzyka i podcasty</li>
        </ul>
        <p className="dim small">Na pozostałych ekranach (rozmowa, kontakty, przypomnienia) przy przyciskach są podpisy.</p>
      </div>
    </main>
  );
}
