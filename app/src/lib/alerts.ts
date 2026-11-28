// Kiedy powiadomić rodzinę (czysta logika, testowana). Dokumentacja v2.0: "Aplikacja rodziny ostrzega przy 20%".
// Alarmujemy tylko przy przejściu progu, nie przy każdym raporcie - powiadomienia nie mogą męczyć.
import type { DeviceStatus } from "./types";

export interface Alert {
  title: string;
  body: string;
}

export const OFFLINE_AFTER_MS = 2 * 60 * 60 * 1000;

const BATTERY_LEVELS = [20, 10];  // od najwyższego
const NONE = 101;                 // żaden próg nie zgłoszony
const REARM = 5;                  // o tyle musi wzrosnąć, żeby ten sam próg zgłosić znowu

/**
 * Najniższy zgłoszony próg baterii i ewentualny nowy alarm. Procent liczony z napięcia skacze
 * (zwłaszcza gdy modem nadaje): 21-20-21-20 nie może dawać powiadomienia za każdym razem.
 * Próg uzbraja się ponownie po ładowaniu albo po wzroście o REARM punktów.
 */
export function batteryAlert(prev: DeviceStatus | null, next: DeviceStatus): { level: number; alert: number | null } {
  let level = prev?.alertLevel;
  if (level === undefined) {
    // raporty sprzed tej zmiany: zgłoszony był próg, poniżej którego już był poprzedni odczyt
    const was = prev?.battery;
    level = was === undefined ? NONE : ([...BATTERY_LEVELS].reverse().find((l) => was <= l) ?? NONE);
  }
  const now = next.battery;
  if (now === undefined) return { level, alert: null };
  if (next.charging) return { level: NONE, alert: null };
  while (level !== NONE && now >= level + REARM) {
    const i = BATTERY_LEVELS.indexOf(level);
    level = i > 0 ? BATTERY_LEVELS[i - 1] : NONE;
  }
  const crossed = [...BATTERY_LEVELS].reverse().find((l) => now <= l && l < level!);
  return crossed === undefined ? { level, alert: null } : { level: crossed, alert: crossed };
}

export function statusAlerts(prev: DeviceStatus | null, next: DeviceStatus, name: string): Alert[] {
  const out: Alert[] = [];
  if (batteryAlert(prev, next).alert !== null)
    out.push({ title: `${name}: słaba bateria (${Math.round(next.battery!)}%)`, body: "Przypomnij o podłączeniu ładowarki - rozładowane LUMI nie odbierze rozmowy." });
  if (prev?.sim !== false && next.sim === false)
    out.push({ title: `${name}: brak karty SIM`, body: "LUMI nie może dzwonić ani odbierać rozmów. Sprawdź kartę SIM." });
  if (prev?.registered !== false && next.registered === false && next.sim !== false && prev)
    out.push({ title: `${name}: brak zasięgu`, body: "LUMI nie łączy się z siecią komórkową. Rozmowy nie dochodzą." });
  return out;
}

export function offlineAlert(name: string): Alert {
  return { title: `${name}: brak połączenia od 2 godzin`, body: "Sprawdź, czy LUMI jest włączone i naładowane. Rozmowy mogą nie dochodzić." };
}

export function backOnlineAlert(name: string): Alert {
  return { title: `${name}: znów w sieci`, body: "LUMI połączyło się ponownie." };
}

export function silentReminderAlert(name: string, items: { text: string; time: string }[]): Alert {
  const list = items.map((r) => `„${r.text}” (${r.time})`).join(", ");
  return {
    title: `${name}: nie wiadomo, czy było przypomnienie`,
    body: `${list} - LUMI nie łączy się od wcześniej, więc mogło nie przypomnieć. Warto zadzwonić.`,
  };
}
