import { addReminder, deleteReminder } from "../actions";
import { requireMember } from "@/lib/access";
import { daysText } from "@/lib/reminders";
import { currentSettings } from "@/lib/store";
import { Flash, Top, type SP } from "@/components/ui";

const DAYS = ["pn", "wt", "śr", "cz", "pt", "sb", "nd"];

export default async function Reminders({ params, searchParams }: { params: Promise<{ id: string }>; searchParams: SP }) {
  const { id } = await params;
  const { ok, err } = await searchParams;
  const { device } = await requireMember(id, "reminders");
  const s = currentSettings(device.settings);
  return (
    <main>
      <Top back={{ href: `/d/${id}`, label: device.name ?? id }} />
      <h1>Przypomnienia</h1>
      <p className="dim">
        LUMI pokaże tekst na cały ekran z dźwiękiem, aż ktoś naciśnie „Zrobione”. Można odłożyć dwa razy po 10 minut. Gdy nikt
        nie potwierdzi, dostaniesz powiadomienie.
      </p>
      <Flash ok={ok} err={err} />
      <ul className="list card">
        {s.reminders.length === 0 && <li className="dim" style={{ padding: 8 }}>Brak przypomnień.</li>}
        {[...s.reminders].sort((a, b) => a.time.localeCompare(b.time)).map((r) => (
          <li key={r.id} className="row" style={{ padding: "10px 0" }}>
            <b style={{ fontSize: 22, minWidth: 64 }}>{r.time}</b>
            <div className="grow">
              {r.text}
              <div className="dim small">
                {daysText(r.days)}
                {!r.confirm && " · bez potwierdzania"}
              </div>
            </div>
            <form action={deleteReminder.bind(null, id, r.id)}>
              <button className="btn small danger" aria-label={`Usuń ${r.text}`}>Usuń</button>
            </form>
          </li>
        ))}
      </ul>
      <form action={addReminder.bind(null, id)} className="card">
        <h2 style={{ marginTop: 0 }}>Nowe przypomnienie</h2>
        <label htmlFor="text">Co przypomnieć (krótko)</label>
        <input id="text" name="text" type="text" required maxLength={60} placeholder="np. Weź leki na ciśnienie" />
        <label htmlFor="time">O której</label>
        <input id="time" name="time" type="time" required defaultValue="08:00" />
        <label>W które dni</label>
        <div className="segmented">
          {DAYS.map((d, i) => (
            <label key={d}>
              <input type="checkbox" name={`d${i + 1}`} defaultChecked />
              <span>{d}</span>
            </label>
          ))}
        </div>
        <label className="check">
          <input type="checkbox" name="confirm" defaultChecked />
          <span>Czekaj na „Zrobione”<br /><span className="dim small">Gdy nikt nie potwierdzi w ciągu 10 minut, dostaniesz powiadomienie.</span></span>
        </label>
        <button className="btn primary block">Dodaj</button>
      </form>
    </main>
  );
}
