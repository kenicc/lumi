import { describe, expect, it, vi } from "vitest";
import { statusAlerts } from "@/lib/alerts";
import { memoryDb as fakeDb } from "@/lib/memoryDb";

describe("statusAlerts", () => {
  it("bateria: tylko przy przejściu progu i bez ładowarki", () => {
    expect(statusAlerts({ battery: 25 }, { battery: 19 }, "Mama")).toHaveLength(1);
    expect(statusAlerts({ battery: 19 }, { battery: 18 }, "Mama")).toHaveLength(0);
    expect(statusAlerts({ battery: 15 }, { battery: 9 }, "Mama")[0].title).toBe("Mama: słaba bateria (9%)");
    expect(statusAlerts({ battery: 25 }, { battery: 19, charging: true }, "Mama")).toHaveLength(0);
  });
  it("karta SIM i zasięg", () => {
    expect(statusAlerts({ sim: true, registered: true }, { sim: false }, "Mama")[0].title).toContain("brak karty SIM");
    expect(statusAlerts({ sim: true, registered: true }, { sim: true, registered: false }, "Mama")[0].title).toContain("brak zasięgu");
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
    expect(await r.json()).toEqual({ alerted: 1 });
    r = await cron(new Request("http://x/api/cron/offline", { headers: auth }));
    expect(await r.json()).toEqual({ alerted: 0 });
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
