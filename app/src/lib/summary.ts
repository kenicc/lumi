// Jedno krótkie zdanie o zmianie, które LUMI pokaże użytkownikowi: "Ania dodała kontakt Basia".
// Zasada 5 z dokumentacji: zdalna konfiguracja nie może działać za plecami użytkownika.
import type { Media, Settings } from "./schema";

export type Form = "f" | "m" | "n";

const VERBS = {
  add: { f: "dodała", m: "dodał", n: "dodaje" },
  remove: { f: "usunęła", m: "usunął", n: "usuwa" },
  change: { f: "zmieniła", m: "zmienił", n: "zmienia" },
  pair: { f: "połączyła", m: "połączył", n: "łączy" },
} as const;

const same = (a: unknown, b: unknown) => JSON.stringify(a) === JSON.stringify(b);
const mediaWord = (m: Media) => (m.type === "playlist" ? "muzykę" : "stację");
const list = (names: string[]) => names.join(", ");

export function pairedText(form: Form) {
  return `${VERBS.pair[form]} LUMI z aplikacją`;
}

export function summarize(prev: Settings, next: Settings, form: Form): string {
  const v = (k: keyof typeof VERBS) => VERBS[k][form];
  const parts: string[] = [];

  const prevIds = new Set(prev.contacts.map((c) => c.id));
  const nextIds = new Set(next.contacts.map((c) => c.id));
  const added = next.contacts.filter((c) => !prevIds.has(c.id)).map((c) => c.name);
  const removed = prev.contacts.filter((c) => !nextIds.has(c.id)).map((c) => c.name);
  if (added.length) parts.push(`${v("add")} ${added.length > 1 ? "kontakty" : "kontakt"} ${list(added)}`);
  if (removed.length) parts.push(`${v("remove")} ${removed.length > 1 ? "kontakty" : "kontakt"} ${list(removed)}`);
  if (!added.length && !removed.length && !same(prev.contacts, next.contacts)) {
    const noPhoto = (cs: Settings["contacts"]) => cs.map(({ photo: _p, ...c }) => c);
    const withNewPhoto = next.contacts.filter((c) => c.photo && prev.contacts.find((p) => p.id === c.id)?.photo !== c.photo);
    if (same(noPhoto(prev.contacts), noPhoto(next.contacts)) && withNewPhoto.length)
      parts.push(`${v("add")} zdjęcie: ${list(withNewPhoto.map((c) => c.name))}`);
    else parts.push(`${v("change")} kontakty`);
  }

  const key = (m: Media) => `${m.type}:${m.name}`;
  const prevM = new Set(prev.media.map(key));
  const nextM = new Set(next.media.map(key));
  const mAdded = next.media.filter((m) => !prevM.has(key(m)));
  const mRemoved = prev.media.filter((m) => !nextM.has(key(m)));
  if (mAdded.length) parts.push(`${v("add")} ${mediaWord(mAdded[0])} ${list(mAdded.map((m) => m.name))}`);
  if (mRemoved.length) parts.push(`${v("remove")} ${mediaWord(mRemoved[0])} ${list(mRemoved.map((m) => m.name))}`);
  if (!mAdded.length && !mRemoved.length && !same(prev.media, next.media)) parts.push(`${v("change")} listę radia`);

  const rKey = (r: { id: string }) => r.id;
  const prevR = new Set(prev.reminders.map(rKey));
  const nextR = new Set(next.reminders.map(rKey));
  const rAdded = next.reminders.filter((r) => !prevR.has(r.id));
  const rRemoved = prev.reminders.filter((r) => !nextR.has(r.id));
  if (rAdded.length) parts.push(`${v("add")} przypomnienie ${list(rAdded.map((r) => r.text))}`);
  if (rRemoved.length) parts.push(`${v("remove")} przypomnienie ${list(rRemoved.map((r) => r.text))}`);
  if (!rAdded.length && !rRemoved.length && !same(prev.reminders, next.reminders)) parts.push(`${v("change")} przypomnienia`);

  if (!same(prev.home, next.home)) parts.push(`${v("change")} przyciski na ekranie głównym`);
  if (!same(prev.display, next.display)) parts.push(`${v("change")} wygląd ekranu`);
  if (!same(prev.sound, next.sound)) parts.push(`${v("change")} głośność`);
  if (!same(prev.access, next.access) || !same(prev.features, next.features) || !same(prev.calls, next.calls))
    parts.push(`${v("change")} ustawienia dostępności`);
  if (!same(prev.weather, next.weather)) parts.push(`${v("change")} pogodę na ${next.weather.place || "inną miejscowość"}`);
  if (!same(prev.wifi, next.wifi)) parts.push(`${v("change")} sieci Wi-Fi`);
  if (!same(prev.sync, next.sync)) parts.push(`${v("change")} ustawienia połączenia`);

  if (!parts.length) return `${v("change")} ustawienia`;
  const first = parts[0].length > 60 ? parts[0].slice(0, 57) + "..." : parts[0];
  return parts.length > 1 ? `${first} i inne ustawienia` : first;
}
