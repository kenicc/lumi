// Ustawienia zrobione na samym LUMI (menu urządzenia, bez telefonu) - czysta logika, testowana.
// Urządzenie wysyła operacje, a nie cały dokument (firmware/src/core/LocalEdit.h):
// - sparowane LUMI: zdarzenie "setting" nakładane na aktualne ustawienia rodziny;
// - niesparowane: migawka w hello, z której parowanie robi ustawienia startowe.
import { z } from "zod";
import { CONTACT_COLORS, SettingsSchema, type Settings } from "./schema";

const name = z.string().trim().min(1).max(24);
const number = z.string().trim().regex(/^\+?[0-9 ]{3,20}$/);
const id = z.string().regex(/^[A-Za-z0-9_-]{1,40}$/);

export const SettingOpSchema = z.discriminatedUnion("op", [
  z.object({ op: z.literal("addContact"), id, name, number }),
  z.object({ op: z.literal("removeContact"), id }),
  z.object({ op: z.literal("textSize"), value: z.number().int().min(0).max(2) }),
  z.object({ op: z.literal("ringVolume"), value: z.number().int().min(1).max(10) }),
]);
export type SettingOp = z.infer<typeof SettingOpSchema>;

export const LocalSnapshotSchema = z.object({
  contacts: z.array(z.object({ id: id.optional(), name, number })).max(30).default([]),
  textSize: z.number().int().min(0).max(2).optional(),
  ringVolume: z.number().int().min(1).max(10).optional(),
});
export type LocalSnapshot = z.infer<typeof LocalSnapshotSchema>;

// Ten sam numer niezależnie od zapisu (+48, 0048, 48, spacje); bez kierunkowego = polski.
// Pełny numer, nie końcówka - jak w firmware (at::sameNumber).
export function canonicalNumber(n: string): string {
  const t = n.trim();
  let d = t.replace(/\D/g, "");
  let intl = t.startsWith("+");
  if (!intl && d.startsWith("00")) {
    intl = true;
    d = d.slice(2);
  }
  return !intl && d.length === 9 ? `48${d}` : d;
}
export const sameNumber = (a: string, b: string) => canonicalNumber(a) === canonicalNumber(b);

/** Nowe ustawienia po operacji z LUMI albo null, gdy nic się nie zmienia (np. ponowione zdarzenie). */
export function applySettingOp(s: Settings, op: SettingOp): Settings | null {
  const next = structuredClone(s);
  switch (op.op) {
    case "addContact": {
      if (next.contacts.some((c) => c.id === op.id || sameNumber(c.number, op.number))) return null;
      if (next.contacts.length >= 30) return null;
      const color = CONTACT_COLORS[next.contacts.length % CONTACT_COLORS.length];
      next.contacts.push({ id: op.id, name: op.name, number: op.number.replace(/\s+/g, ""), color, autoAnswer: false });
      break;
    }
    case "removeContact": {
      if (!next.contacts.some((c) => c.id === op.id)) return null;
      next.contacts = next.contacts.filter((c) => c.id !== op.id);
      next.home.slots = next.home.slots.map((x) => (x === `contact:${op.id}` ? "" : x)) as Settings["home"]["slots"];
      break;
    }
    case "textSize":
      if (next.display.textSize === op.value) return null;
      next.display.textSize = op.value;
      break;
    case "ringVolume":
      if (next.sound.ringVolume === op.value) return null;
      next.sound.ringVolume = op.value;
      break;
  }
  const parsed = SettingsSchema.safeParse(next);
  return parsed.success ? parsed.data : null;
}

/** Ustawienia startowe przy parowaniu: to, co senior ustawił na LUMI, zamiast pustej listy. */
export function fromLocal(base: Settings, local: LocalSnapshot | null): Settings {
  if (!local) return base;
  const next = structuredClone(base);
  for (const c of local.contacts) {
    if (next.contacts.some((k) => sameNumber(k.number, c.number))) continue;
    const cid = c.id ?? `d${next.contacts.length}${Date.now().toString(36)}`;
    next.contacts.push({
      id: cid,
      name: c.name,
      number: c.number.replace(/\s+/g, ""),
      color: CONTACT_COLORS[next.contacts.length % CONTACT_COLORS.length],
      autoAnswer: false,
    });
  }
  if (local.textSize !== undefined) next.display.textSize = local.textSize;
  if (local.ringVolume !== undefined) next.sound.ringVolume = local.ringVolume;
  const parsed = SettingsSchema.safeParse(next);
  return parsed.success ? parsed.data : base;
}
