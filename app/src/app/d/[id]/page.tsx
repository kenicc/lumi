import Link from "next/link";
import { can, requireMember, type Section } from "@/lib/access";
import { admin } from "@/lib/supabase/admin";
import { todayPlan, type DayState, type ReminderEvent } from "@/lib/reminders";
import { batteryText, isOnline, signalText } from "@/lib/status";
import { currentSettings } from "@/lib/store";
import { ROLE_NAMES } from "@/lib/types";
import { AutoRefresh } from "@/components/AutoRefresh";
import { Flash, Top, ago, type SP } from "@/components/ui";

const SECTIONS: { href: string; ico: string; title: string; desc: string; section: Section }[] = [
  { href: "kontakty", ico: "📞", title: "Kontakty", desc: "kto jest pod przyciskiem Dzwoń", section: "contacts" },
  { href: "ekran", ico: "🏠", title: "Ekran główny", desc: "trzy przyciski obok zegara", section: "home" },
  { href: "wiadomosci", ico: "💬", title: "Wiadomości", desc: "krótki tekst na ekran LUMI", section: "messages" },
  { href: "przypomnienia", ico: "⏰", title: "Przypomnienia", desc: "leki, posiłki, wizyty - z potwierdzeniem", section: "reminders" },
  { href: "radio", ico: "📻", title: "Radio i muzyka", desc: "stacje FM, internetowe, piosenki", section: "media" },
  { href: "wyglad", ico: "🔆", title: "Wygląd i dźwięk", desc: "wielkość tekstu, głośność, cisza nocna", section: "look" },
  { href: "pogoda", ico: "⛅", title: "Pogoda", desc: "miejscowość", section: "weather" },
  { href: "dostepnosc", ico: "♿", title: "Dostępność", desc: "auto-odbieranie, drżenie dłoni, blokady", section: "access" },
  { href: "wifi", ico: "📶", title: "Wi-Fi", desc: "sieci, z którymi łączy się LUMI", section: "wifi" },
  { href: "osoby", ico: "👥", title: "Osoby", desc: "kto ma dostęp do ustawień", section: "people" },
];

const STATE: Record<DayState, { text: string; cls: string }> = {
  done: { text: "zrobione", cls: "badge ok" },
  missed: { text: "bez potwierdzenia", cls: "badge bad" },
  snoozed: { text: "odłożone", cls: "badge" },
  shown: { text: "przypomniane", cls: "badge ok" },
  waiting: { text: "LUMI przypomina", cls: "badge" },
  upcoming: { text: "później", cls: "dim small" },
  unknown: { text: "brak informacji", cls: "badge bad" },
};
const hm = (iso?: string) =>
  iso ? new Date(iso).toLocaleTimeString("pl-PL", { hour: "2-digit", minute: "2-digit", timeZone: "Europe/Warsaw" }) : "";

export default async function Device({ params, searchParams }: { params: Promise<{ id: string }>; searchParams: SP }) {
  const { id } = await params;
  const { ok, err } = await searchParams;
  const { role, device: d } = await requireMember(id);
  const { data: ch } = await admin()
    .from("changes")
    .select("author, summary, version, created_at")
    .eq("device_id", id)
    .order("created_at", { ascending: false })
    .limit(5);
  const changes = (ch ?? []) as { author: string; summary: string; version: number; created_at: string }[];
  const { data: evs } = await admin()
    .from("reminder_events")
    .select("reminder_id, status, at")
    .eq("device_id", id)
    .gt("at", new Date(Date.now() - 36 * 3600 * 1000).toISOString());
  const plan = todayPlan(currentSettings(d.settings).reminders, (evs ?? []) as ReminderEvent[]);
  const online = isOnline(d);
  const pending = d.applied_version < d.config_version;

  return (
    <main>
      <AutoRefresh />
      <Top back={{ href: "/", label: "Urządzenia" }} title={ROLE_NAMES[role]} />
      <h1>{d.name ?? d.id}</h1>
      <Flash ok={ok} err={err} />
      <section className="card">
        <div className="row">
          <span className={`badge ${online ? "ok" : "bad"}`}>{online ? "w sieci" : "poza siecią"}</span>
          <span className="dim small grow">ostatni kontakt {ago(d.last_seen)}</span>
        </div>
        <div className="stats" style={{ marginTop: 12 }}>
          <div className="stat"><b>{batteryText(d)}</b><span className="small dim">bateria</span></div>
          <div className="stat"><b>{signalText(d)}</b><span className="small dim">zasięg{d.status?.lte ? " LTE" : ""}</span></div>
          <div className="stat"><b>{pending ? "czeka" : "✓"}</b><span className="small dim">zmiany</span></div>
        </div>
        {pending && (
          <p className="small dim">
            Ostatnia zmiana czeka na LUMI. {online ? "Powinna dotrzeć w ciągu minuty." : "Dotrze, gdy LUMI połączy się z siecią."}
          </p>
        )}
        {(d.status?.battery ?? 100) <= 20 && !d.status?.charging && <p className="flash err">Słaba bateria - przypomnij o ładowaniu.</p>}
      </section>

      {plan.length > 0 && (
        <section className="card">
          <h2 style={{ marginTop: 0 }}>Dziś</h2>
          <ul className="list">
            {plan.map((p) => (
              <li key={p.reminder.id} className="row" style={{ padding: "8px 0" }}>
                <b style={{ minWidth: 56 }}>{p.reminder.time}</b>
                <span className="grow">{p.reminder.text}</span>
                <span className={STATE[p.state].cls}>
                  {STATE[p.state].text}
                  {p.state === "done" && p.at ? ` ${hm(p.at)}` : ""}
                </span>
              </li>
            ))}
          </ul>
        </section>
      )}

      <ul className="list nav card">
        {SECTIONS.filter((s) => can(role, s.section)).map((s) => (
          <li key={s.href}>
            <Link href={`/d/${id}/${s.href}`}>
              <span className="ico" aria-hidden>{s.ico}</span>
              <span>
                <b>{s.title}</b>
                <br />
                <span className="dim small">{s.desc}</span>
              </span>
            </Link>
          </li>
        ))}
      </ul>

      {changes.length > 0 && (
        <>
          <h2>Ostatnie zmiany</h2>
          <ul className="list card">
            {changes.map((c) => (
              <li key={c.version} style={{ padding: "10px 0" }}>
                <b>{c.author}</b> {c.summary}
                <div className="dim small">
                  {ago(c.created_at)} · {c.version <= d.applied_version ? "jest na LUMI" : "czeka na LUMI"}
                </div>
              </li>
            ))}
          </ul>
        </>
      )}
      {role === "guardian" ? (
        <details className="card">
          <summary>Diagnostyka</summary>
          <ul className="small" style={{ paddingLeft: 18 }}>
            <li>{d.id} · {d.variant ?? "?"} · oprogramowanie {d.fw ?? "?"} (build {d.fw_build})</li>
            <li>modem: {d.status?.modem ?? "?"}{d.status?.operator ? `, ${d.status.operator}` : ""}</li>
            <li>
              uruchomienie nr {d.status?.boots ?? "?"}, ostatni restart: {d.status?.reset ?? "?"}
              {d.status?.uptime !== undefined && `, działa od ${Math.round(d.status.uptime / 3600)} godz.`}
            </li>
            {d.status?.wifiRssi !== undefined && <li>Wi-Fi: {d.status.wifiRssi} dBm</li>}
          </ul>
          {(d.status?.errors?.length ?? 0) > 0 && (
            <>
              <b className="small">Ostatnie błędy</b>
              <pre className="small" style={{ whiteSpace: "pre-wrap", margin: "6px 0 0" }}>{d.status!.errors!.join("\n")}</pre>
            </>
          )}
        </details>
      ) : (
        <p className="dim small">{d.id} · oprogramowanie {d.fw ?? "?"}</p>
      )}
    </main>
  );
}
