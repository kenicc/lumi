import { describe, expect, it, vi } from "vitest";
import { batteryAlert, signalAlert, statusAlerts } from "@/lib/alerts";
import type { DeviceStatus } from "@/lib/types";
import { memoryDb as fakeDb } from "@/lib/memoryDb";

describe("statusAlerts", () => {
  it("bateria: tylko przy przejściu progu i bez ładowarki", () => {
    expect(statusAlerts({ battery: 25 }, { battery: 19 }, "Mama")).toHaveLength(1);
    expect(statusAlerts({ battery: 19 }, { battery: 18 }, "Mama")).toHaveLength(0);
    expect(statusAlerts({ battery: 15 }, { battery: 9 }, "Mama")[0].title).toBe("Mama: słaba bateria (9%)");
    expect(statusAlerts({ battery: 25 }, { battery: 19, charging: true }, "Mama")).toHaveLength(0);
  });
  it("bateria: skaczący odczyt 21-20-21-20 daje jedno powiadomienie, nie cztery", () => {
    let prev: DeviceStatus | null = { battery: 30 };
    let count = 0;
    for (const b of [21, 20, 21, 20, 22, 19, 21, 20]) {
      const next: DeviceStatus = { battery: b };
      count += statusAlerts(prev, next, "Mama").length;
      prev = { ...next, alertLevel: batteryAlert(prev, next).level };
    }
    expect(count).toBe(1);
    // dalszy spadek do 10%: drugi próg
    expect(statusAlerts(prev, { battery: 10 }, "Mama")).toHaveLength(1);
  });
  it("bateria: próg uzbraja się po ładowaniu albo po wzroście o 5 punktów", () => {
    expect(batteryAlert({ battery: 20, alertLevel: 20 }, { battery: 24 }).level).toBe(20);
    expect(batteryAlert({ battery: 20, alertLevel: 20 }, { battery: 25 }).level).toBe(101);
    expect(batteryAlert({ battery: 19, alertLevel: 20 }, { battery: 19, charging: true }).level).toBe(101);
    expect(batteryAlert({ battery: 10, alertLevel: 10 }, { battery: 15 }).level).toBe(20);
    // po uzbrojeniu kolejny spadek znowu powiadamia
    expect(batteryAlert({ battery: 26, alertLevel: 101 }, { battery: 20 }).alert).toBe(20);
    // skok 30 -> 8: jedno powiadomienie, od razu o niższym progu
    expect(batteryAlert({ battery: 30 }, { battery: 8 })).toEqual({ level: 10, alert: 10 });
  });
  it("karta SIM i zasięg", () => {
    expect(statusAlerts({ sim: true, registered: true }, { sim: false }, "Mama")[0].title).toContain("brak karty SIM");
    // brak zasięgu: dopiero po 10 minutach i raz na przerwę
    const t0 = Date.parse("2026-11-25T10:00:00Z");
    const lost = { sim: true, registered: false };
    expect(statusAlerts({ sim: true, registered: true }, lost, "Mama", t0)).toHaveLength(0);
    const s1 = { ...lost, ...signalAlert({ sim: true, registered: true }, lost, t0) };
    const p1: DeviceStatus = { ...lost, lostAt: s1.lostAt, signalAlerted: s1.alerted };
    expect(statusAlerts(p1, lost, "Mama", t0 + 5 * 60_000)).toHaveLength(0);
    expect(statusAlerts(p1, lost, "Mama", t0 + 10 * 60_000)[0].title).toContain("brak zasięgu");
    const p2: DeviceStatus = { ...p1, signalAlerted: true };
    expect(statusAlerts(p2, lost, "Mama", t0 + 20 * 60_000)).toHaveLength(0);
    // zasięg wrócił na chwilę - nowa przerwa liczy się od nowa
    expect(signalAlert(p2, { sim: true, registered: true }, t0 + 21 * 60_000)).toMatchObject({ alerted: false, lostAt: undefined });
    expect(statusAlerts(null, { sim: true, registered: false }, "Mama")).toHaveLength(0); // pierwszy raport po starcie
  });
});

vi.mock("server-only", () => ({}));
const db = fakeDb();
vi.mock("@/lib/supabase/admin", () => ({ admin: () => db.client }));
const sent: { id: string; title: string }[] = [];
vi.mock("@/lib/push", () => ({ notifyDevice: async (id: string, a: { title: string }) => (sent.push({ id, title: a.title }), 1) }));

describe("powiadomienia z serwera", async () => {
  const { GET: cron } = await import("@/app/api/cron/offline/route");
  const { POST: status } = await import("@/app/api/device/status/route");
  const { sha256 } = await import("@/lib/deviceAuth");
  process.env.CRON_SECRET = "s3cret";
  const old = new Date(Date.now() - 3 * 3600 * 1000).toISOString();
  const auth = { Authorization: "Bearer s3cret" };

  it("przypomnienie bez potwierdzenia: zapis i powiadomienie; potwierdzone: bez powiadomienia", async () => {
    const { POST: event } = await import("@/app/api/device/event/route");
    db.tables.devices.push({
      id: "LUMI-000003", name: "Tata", secret_hash: sha256("b".repeat(64)), config_version: 1, fw_build: 4,
      settings: { reminders: [{ id: "leki", text: "Weź leki", time: "08:00", days: [], confirm: true }] },
    });
    const send = (status: string) =>
      event(new Request("http://x/api/device/event", {
        method: "POST",
        headers: { Authorization: `Bearer ${"b".repeat(64)}`, "X-Lumi-Device": "LUMI-000003", "Content-Type": "application/json" },
        body: JSON.stringify({ type: "reminder", id: "leki", text: "Weź leki", minute: 480, status, at: Math.floor(Date.now() / 1000) }),
      }));
    const before = sent.length;
    expect((await send("done")).status).toBe(200);
    expect(sent.length).toBe(before);
    expect((await send("missed")).status).toBe(200);
    expect(sent.at(-1)!.title).toBe("Tata: przypomnienie bez potwierdzenia");
    expect(db.tables.reminder_events.filter((e) => e.device_id === "LUMI-000003")).toHaveLength(2);
  });

  it("cron: raz na przerwę, a po powrocie 'znów w sieci'", async () => {
    const start = sent.length;
    db.tables.devices.push(
      { id: "LUMI-000001", name: "Mama", secret_hash: sha256("a".repeat(64)), config_version: 2, applied_version: 2, last_seen: old, offline_alerted: false, status: { battery: 50 }, fw_build: 4 },
      { id: "LUMI-000002", name: "Nowe", config_version: 0, last_seen: old, offline_alerted: false },
    );
    expect((await cron(new Request("http://x/api/cron/offline"))).status).toBe(401);
    let r = await cron(new Request("http://x/api/cron/offline", { headers: auth }));
    expect(await r.json()).toMatchObject({ alerted: 1 });
    r = await cron(new Request("http://x/api/cron/offline", { headers: auth }));
    expect(await r.json()).toMatchObject({ alerted: 0 });
    expect(sent.slice(start).map((s) => s.title)).toEqual(["Mama: brak połączenia od 2 godzin"]);

    const req = new Request("http://x/api/device/status", {
      method: "POST",
      headers: { Authorization: `Bearer ${"a".repeat(64)}`, "X-Lumi-Device": "LUMI-000001", "Content-Type": "application/json" },
      body: JSON.stringify({ version: 2, battery: 18, charging: false }),
    });
    expect((await status(req)).status).toBe(200);
    expect(sent.slice(start + 1).map((s) => s.title)).toEqual(["Mama: słaba bateria (18%)", "Mama: znów w sieci"]);
    expect(db.tables.devices.find((d) => d.id === "LUMI-000001")!.offline_alerted).toBe(false);
  });
});
