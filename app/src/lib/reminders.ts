// Plan przypomnień na dziś (strefa czasowa urządzenia: Europe/Warsaw) i ich stan z raportów LUMI.
import type { Reminder } from "./schema";

export interface ReminderEvent {
  reminder_id: string;
  // nodata: wpis serwera - LUMI nie dało znać o przypomnieniu, rodzina dostała powiadomienie
  status: "done" | "snoozed" | "missed" | "shown" | "nodata";
  at: string; // ISO
}

export type DayState = "done" | "missed" | "snoozed" | "shown" | "waiting" | "upcoming" | "unknown";

const DAY_NAMES = ["pn", "wt", "śr", "cz", "pt", "sb", "nd"];

const EVERY = ["poniedziałek", "wtorek", "środę", "czwartek", "piątek", "sobotę", "niedzielę"];

export function daysText(days: number[]): string {
  const d = [...new Set(days)].sort();
  if (d.length === 0 || d.length === 7) return "codziennie";
  if (d.join() === "1,2,3,4,5") return "pn-pt";
  if (d.join() === "6,7") return "weekend";
  if (d.length === 1) return `co ${EVERY[d[0] - 1]}`;  // samo "śr" wygląda jak skrót, którego nikt nie rozszyfruje
  return d.map((x) => DAY_NAMES[x - 1]).join(", ");
}

// Data, dzień tygodnia (1 = pn) i minuta doby w Polsce
export function warsawNow(now = new Date()) {
  const f = new Intl.DateTimeFormat("en-GB", { timeZone: "Europe/Warsaw", year: "numeric", month: "2-digit", day: "2-digit", weekday: "short", hour: "2-digit", minute: "2-digit", hourCycle: "h23" });
  const p = Object.fromEntries(f.formatToParts(now).map((x) => [x.type, x.value]));
  const wd = ["Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"].indexOf(p.weekday) + 1;
  return { date: `${p.year}-${p.month}-${p.day}`, weekday: wd, minute: Number(p.hour) * 60 + Number(p.minute) };
}

export const toMinute = (hm: string) => Number(hm.slice(0, 2)) * 60 + Number(hm.slice(3, 5));

export function todayPlan(reminders: Reminder[], events: ReminderEvent[], now = new Date()) {
  const w = warsawNow(now);
  const today = (iso: string) => warsawNow(new Date(iso)).date === w.date;
  return reminders
    .filter((r) => r.days.length === 0 || r.days.includes(w.weekday))
    .map((r) => {
      // raport z LUMI ma pierwszeństwo przed wpisem "nodata" serwera (urządzenie mogło wrócić później)
      const ev = events
        .filter((e) => e.reminder_id === r.id && today(e.at) && e.status !== "nodata")
        .sort((a, b) => a.at.localeCompare(b.at))
        .at(-1);
      const m = toMinute(r.time);
      let state: DayState;
      if (ev) state = ev.status as DayState;
      else if (m > w.minute) state = "upcoming";
      else if (w.minute - m < 15) state = "waiting"; // LUMI właśnie przypomina
      else state = "unknown"; // brak raportu - np. LUMI było wyłączone albo bez internetu
      return { reminder: r, state, at: ev?.at };
    })
    .sort((a, b) => a.reminder.time.localeCompare(b.reminder.time));
}

export const SILENT_AFTER_MIN = 45;  // tyle czasu LUMI ma na pokazanie, 2 odłożenia i raport
const SILENT_WINDOW_MIN = 240;      // starszych nie zgłaszamy (np. po pierwszym wdrożeniu)

/**
 * Przypomnienia, o których LUMI nic nie powiedziało, choć dawno minęła ich pora - a samo LUMI nie
 * łączy się od chwili przed tą porą (wyłączone, rozładowane, bez zasięgu). Wtedy urządzenie nie wyśle
 * "bez potwierdzenia", więc rodzinę musi zawiadomić serwer. Każde zgłaszamy raz (wpis "nodata").
 */
export function silentReminders(reminders: Reminder[], events: ReminderEvent[], lastSeen: string | null, now = new Date()) {
  const w = warsawNow(now);
  const today = (iso: string) => warsawNow(new Date(iso)).date === w.date;
  return reminders.filter((r) => {
    if (r.days.length && !r.days.includes(w.weekday)) return false;
    const ago = w.minute - toMinute(r.time);
    if (ago < SILENT_AFTER_MIN || ago > SILENT_WINDOW_MIN) return false;
    if (events.some((e) => e.reminder_id === r.id && today(e.at))) return false;
    const due = now.getTime() - ago * 60_000;
    return !lastSeen || new Date(lastSeen).getTime() < due;
  });
}
