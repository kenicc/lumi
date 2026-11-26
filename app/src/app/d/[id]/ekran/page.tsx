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
  const label = (slot: string) => {
    if (slot === "media") return "Radio";
    if (slot === "weather") return "Pogoda";
    const c = s.contacts.find((k) => `contact:${k.id}` === slot);
    return c ? c.name : "";
  };
  const options = [
    { v: "", t: "(pusty)" },
    ...s.contacts.map((c) => ({ v: `contact:${c.id}`, t: `Zadzwoń: ${c.name}` })),
    { v: "media", t: "Radio i muzyka" },
    { v: "weather", t: "Pogoda na jutro" },
  ];

  return (
    <main>
      <Top back={{ href: `/d/${id}`, label: device.name ?? id }} />
      <h1>Ekran główny</h1>
      <p className="dim">
        Obok zegara są trzy przyciski. Najczęściej: osoba, do której dzwoni się najczęściej, radio i pogoda. Szybkie
        wybieranie zawsze pyta „Zadzwonić?”, żeby LUMI nie zadzwoniło przypadkiem z kieszeni.
      </p>
      <Flash ok={ok} err={err} />
      {/* podgląd: tak to wygląda na LUMI */}
      <div className="card" style={{ background: "#0b0b0d", color: "#f4f1ea", border: 0, display: "grid", gridTemplateColumns: "1fr 110px", gap: 8 }}>
        <div>
          <div style={{ fontSize: 46, fontWeight: 600, lineHeight: 1, letterSpacing: "-0.02em", fontVariantNumeric: "tabular-nums" }}>10:24</div>
          <div style={{ fontWeight: 700 }}>Wtorek</div>
          {s.home.partOfDay && <div style={{ color: "#f2b04b" }}>rano</div>}
          <div style={{ color: "#a39e94" }}>24 listopada</div>
        </div>
        <div style={{ display: "grid", gap: 6 }}>
          {s.home.slots.map((slot, i) => (
            <div
              key={i}
              style={{
                borderRadius: 12, padding: "8px 6px", textAlign: "center", fontWeight: 700, minHeight: 40,
                background: slot.startsWith("contact:") ? "#1d7350" : slot ? "#1c1d21" : "transparent",
              }}
            >
              {label(slot)}
            </div>
          ))}
        </div>
      </div>
      <form action={saveHome.bind(null, id)} className="card">
        {POS.map((name, i) => (
          <div key={i}>
            <label htmlFor={`slot${i}`}>{name} przycisk</label>
            <select id={`slot${i}`} name={`slot${i}`} defaultValue={s.home.slots[i]}>
              {options.map((o) => <option key={o.v} value={o.v}>{o.t}</option>)}
            </select>
          </div>
        ))}
        <label className="check">
          <input type="checkbox" name="partOfDay" defaultChecked={s.home.partOfDay} />
          <span>Pokazuj porę dnia („Wtorek wieczorem”)<br /><span className="dim small">Pomaga w orientacji, gdy dni się mylą.</span></span>
        </label>
        <SaveBar />
      </form>
    </main>
  );
}
