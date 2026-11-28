import { saveLook } from "../actions";
import { requireMember } from "@/lib/access";
import { currentSettings } from "@/lib/store";
import { Flash, SaveBar, Top, type SP } from "@/components/ui";

const SIZES = [
  { v: 0, t: "Normalny", px: 18 },
  { v: 1, t: "Duży", px: 22 },
  { v: 2, t: "Bardzo duży", px: 26 },
];

export default async function Look({ params, searchParams }: { params: Promise<{ id: string }>; searchParams: SP }) {
  const { id } = await params;
  const { ok, err } = await searchParams;
  const { device } = await requireMember(id, "look");
  const s = currentSettings(device.settings);
  return (
    <main>
      <Top back={{ href: `/d/${id}`, label: device.name ?? id }} />
      <h1>Wygląd i dźwięk</h1>
      <Flash ok={ok} err={err} />
      <form action={saveLook.bind(null, id)}>
        <section className="card">
          <h2 style={{ marginTop: 0 }}>Ekran</h2>
          <label>Wielkość tekstu</label>
          <div className="segmented">
            {SIZES.map((z) => (
              <label key={z.v}>
                <input type="radio" name="textSize" value={z.v} defaultChecked={s.display.textSize === z.v} />
                <span style={{ fontSize: z.px }}>{z.t}</span>
              </label>
            ))}
          </div>
          <label className="check">
            <input type="checkbox" name="highContrast" defaultChecked={s.display.highContrast} />
            <span>Wysoki kontrast<br /><span className="dim small">Biel na czerni i grube obramowania - przy słabym wzroku.</span></span>
          </label>
          <label htmlFor="brightness">Jasność: {s.display.brightness}%</label>
          <input id="brightness" name="brightness" type="range" min={10} max={100} step={10} defaultValue={s.display.brightness} />
          <label htmlFor="screenOffSec">Wygaszanie ekranu</label>
          <select id="screenOffSec" name="screenOffSec" defaultValue={s.display.screenOffSec}>
            {[30, 60, 120, 300, 600].map((v) => <option key={v} value={v}>{v < 60 ? `${v} s` : `${v / 60} min`}</option>)}
          </select>
        </section>
        <section className="card">
          <h2 style={{ marginTop: 0 }}>Dźwięk</h2>
          <label htmlFor="ringVolume">Głośność dzwonka: {s.sound.ringVolume} / 10</label>
          <input id="ringVolume" name="ringVolume" type="range" min={1} max={10} defaultValue={s.sound.ringVolume} />
          <label htmlFor="startVolume">Głośność radia po włączeniu: {s.sound.startVolume} / 10</label>
          <input id="startVolume" name="startVolume" type="range" min={1} max={10} defaultValue={s.sound.startVolume} />
          <label htmlFor="maxVolume">Najwyższa głośność rolki: {s.sound.maxVolume} / 10</label>
          <input id="maxVolume" name="maxVolume" type="range" min={1} max={10} defaultValue={s.sound.maxVolume} />
          <label className="check">
            <input type="checkbox" name="keyClick" defaultChecked={s.sound.keyClick} /> Dźwięk przy naciśnięciu przycisku
          </label>
          <label className="check">
            <input type="checkbox" name="quietOn" defaultChecked={!!s.sound.quiet} />
            <span>Godziny ciszy<br /><span className="dim small">Dzwonek jest wtedy cichy, ale nigdy wyłączony - rodzina zawsze się dodzwoni.</span></span>
          </label>
          <div className="row">
            <div className="grow"><label htmlFor="qf">od</label><input id="qf" name="quietFrom" type="time" defaultValue={s.sound.quiet?.from ?? "22:00"} /></div>
            <div className="grow"><label htmlFor="qt">do</label><input id="qt" name="quietTo" type="time" defaultValue={s.sound.quiet?.to ?? "07:00"} /></div>
          </div>
        </section>
        <SaveBar />
      </form>
    </main>
  );
}
