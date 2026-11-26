// Kiedy powiadomić rodzinę (czysta logika, testowana). Dokumentacja v2.0: "Aplikacja rodziny ostrzega przy 20%".
// Alarmujemy tylko przy przejściu progu, nie przy każdym raporcie - powiadomienia nie mogą męczyć.
import type { DeviceStatus } from "./types";

export interface Alert {
  title: string;
  body: string;
}

export const OFFLINE_AFTER_MS = 2 * 60 * 60 * 1000;

export function statusAlerts(prev: DeviceStatus | null, next: DeviceStatus, name: string): Alert[] {
  const out: Alert[] = [];
  const was = prev?.battery;
  const now = next.battery;
  if (now !== undefined && !next.charging) {
    for (const level of [20, 10]) {
      if (now <= level && (was === undefined || was > level)) {
        out.push({ title: `${name}: słaba bateria (${Math.round(now)}%)`, body: "Przypomnij o podłączeniu ładowarki - rozładowane LUMI nie odbierze rozmowy." });
        break;
      }
    }
  }
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
