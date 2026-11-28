import { describe, expect, it, vi } from "vitest";
import { memoryDb } from "@/lib/memoryDb";
import { silentReminders, todayPlan, type ReminderEvent } from "@/lib/reminders";
import type { Reminder } from "@/lib/schema";
import { sample } from "./fixtures";

vi.mock("server-only", () => ({}));
const db = memoryDb();
vi.mock("@/lib/supabase/admin", () => ({ admin: () => db.client }));
const sent: { id: string; title: string; body: string }[] = [];
vi.mock("@/lib/push", () => ({
  notifyDevice: async (id: string, a: { title: string; body: string }) => {
    sent.push({ id, ...a });
    return 1;
  },
}));

// środa 25.11.2026, 9:00 w Warszawie (UTC+1)
const NOW = new Date("2026-11-25T08:00:00Z");
const r = (id: string, time: string, days: number[] = []): Reminder => ({ id, text: `Leki ${time}`, time, days, confirm: true });
const ev = (reminder_id: string, status: ReminderEvent["status"], iso: string): ReminderEvent => ({ reminder_id, status, at: iso });
const beforeDue = "2026-11-25T06:50:00Z";  // 7:50, przed przypomnieniem o 8:00

describe("przypomnienia bez wieści od LUMI", () => {
  it("zgłasza tylko te po czasie, bez raportu, gdy LUMI milczy od przed ich porą", () => {
    const list = [r("a", "08:00"), r("b", "08:30"), r("c", "05:00"), r("d", "08:00", [1]), r("e", "10:00")];
    // b: tylko 30 min po czasie; c: ponad 4 h; d: nie w środę; e: jeszcze nie pora
    expect(silentReminders(list, [], beforeDue, NOW).map((x) => x.id)).toEqual(["a"]);
    // LUMI łączyło się po 8:00 - samo zgłosi "bez potwierdzenia", serwer nie dubluje
    expect(silentReminders(list, [], "2026-11-25T07:30:00Z", NOW)).toEqual([]);
    // jest raport albo już zgłoszone
    expect(silentReminders(list, [ev("a", "shown", "2026-11-25T07:01:00Z")], beforeDue, NOW)).toEqual([]);
    expect(silentReminders(list, [ev("a", "nodata", "2026-11-25T07:45:00Z")], beforeDue, NOW)).toEqual([]);
    // wczorajszy raport się nie liczy
    expect(silentReminders(list, [ev("a", "done", "2026-11-24T07:05:00Z")], beforeDue, NOW).map((x) => x.id)).toEqual(["a"]);
  });

  it("raport z LUMI, który przyszedł później, wygrywa na pulpicie z wpisem serwera", () => {
    const plan = todayPlan([r("a", "08:00")], [ev("a", "nodata", "2026-11-25T07:45:00Z"), ev("a", "done", "2026-11-25T07:05:00Z")], NOW);
    expect(plan[0].state).toBe("done");
    expect(todayPlan([r("a", "08:00")], [ev("a", "nodata", "2026-11-25T07:45:00Z")], NOW)[0].state).toBe("unknown");
  });

  it("harmonogram: jedno powiadomienie na przypomnienie, także przy kolejnych uruchomieniach", async () => {
    const { alertSilentReminders } = await import("@/lib/silentReminders");
    const s = sample();
    s.reminders = [r("a", "08:00")];
    db.tables.devices.push({ id: "L1", name: "Mama", settings: s, config_version: 3, last_seen: beforeDue });
    expect(await alertSilentReminders(NOW)).toBe(1);
    expect(sent).toHaveLength(1);
    expect(sent[0].title).toContain("Mama");
    expect(sent[0].body).toContain("Leki 08:00");
    expect(db.tables.reminder_events.at(-1)).toMatchObject({ reminder_id: "a", status: "nodata" });
    expect(await alertSilentReminders(new Date(NOW.getTime() + 30 * 60_000))).toBe(0);
    expect(sent).toHaveLength(1);
  });

  it("gdy wpisu nie da się zapisać (stara baza), nie wysyła - inaczej powtarzałby co godzinę", async () => {
    const { alertSilentReminders } = await import("@/lib/silentReminders");
    const s = sample();
    s.reminders = [r("x", "08:00")];
    db.tables.devices.splice(0, db.tables.devices.length, { id: "L2", name: "Tata", settings: s, config_version: 1, last_seen: beforeDue });
    const client = db.client as unknown as { from: (t: string) => Record<string, unknown> };
    const orig = client.from.bind(client);
    client.from = (t: string) => {
      const q = orig(t);
      return t === "reminder_events" ? { ...q, insert: async () => ({ error: { message: "violates check constraint" } }) } : q;
    };
    const before = sent.length;
    try {
      expect(await alertSilentReminders(NOW)).toBe(0);
    } finally {
      client.from = orig;
    }
    expect(sent.length).toBe(before);
  });
});
