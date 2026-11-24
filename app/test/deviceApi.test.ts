// Protokół urządzenia (firmware/src/net/Net.cpp) przeciw prawdziwym trasom API na atrapie bazy
import { beforeEach, describe, expect, it, vi } from "vitest";
import { fakeDb } from "./fakeDb";

vi.mock("server-only", () => ({}));
const db = fakeDb();
vi.mock("@/lib/supabase/admin", () => ({ admin: () => db.client }));

const { POST: hello } = await import("@/app/api/device/hello/route");
const { GET: config } = await import("@/app/api/device/config/route");
const { POST: status } = await import("@/app/api/device/status/route");

const ID = "LUMI-C3D4E5";
const SECRET = "a".repeat(64);
const PAIR = "K7MPQ2XR";

const headers = (secret = SECRET, version = 0) => ({
  Authorization: `Bearer ${secret}`,
  "X-Lumi-Device": ID,
  "X-Lumi-Version": String(version),
  "Content-Type": "application/json",
});
const post = (url: string, body: unknown, h = headers()) =>
  new Request(`http://x${url}`, { method: "POST", headers: h, body: JSON.stringify(body) });
const get = (url: string, h = headers()) => new Request(`http://x${url}`, { headers: h });

beforeEach(() => {
  for (const t of Object.keys(db.tables)) db.tables[t] = [];
});

describe("API urządzenia", () => {
  it("rejestracja, parowanie, konfiguracja i stan", async () => {
    let r = await hello(post("/api/device/hello", { id: ID, secret: SECRET, pair: PAIR, variant: "allegro", fw: "Nov 24 2026" }));
    expect(r.status).toBe(200);
    expect(await r.json()).toEqual({ claimed: false });
    expect(db.tables.devices[0].pair_hash).toBeTruthy();

    // przed dodaniem w aplikacji urządzenie słyszy "nieprzypisane" i pokazuje kod parowania
    r = await config(get("/api/device/config"));
    expect(r.status).toBe(409);

    // zły sekret
    r = await config(get("/api/device/config", headers("b".repeat(64))));
    expect(r.status).toBe(401);
    r = await hello(post("/api/device/hello", { id: ID, secret: "b".repeat(64), pair: PAIR }));
    expect(r.status).toBe(403);

    // parowanie (to robi akcja claim): opiekun + pierwsza konfiguracja
    db.tables.members.push({ device_id: ID, user_id: "u1", role: "guardian" });
    Object.assign(db.tables.devices[0], { config: { version: 1, changedBy: "Ania" }, config_version: 1 });

    r = await config(get("/api/device/config"));
    expect(r.status).toBe(200);
    expect((await r.json()).version).toBe(1);
    r = await config(get("/api/device/config", headers(SECRET, 1)));
    expect(r.status).toBe(304);

    r = await status(post("/api/device/status", { version: 1, battery: 76, signal: 3, registered: true, lte: false, modem: "SIM800 (2G)", operator: "Orange", sim: true, charging: false, batteryMv: 3950, wifiRssi: -60, uptime: 120, variant: "allegro", fw: "Nov 24 2026" }));
    expect(r.status).toBe(200);
    expect(db.tables.devices[0].applied_version).toBe(1);
    expect((db.tables.devices[0].status as { battery: number }).battery).toBe(76);

    r = await hello(post("/api/device/hello", { id: ID, secret: SECRET, pair: PAIR }));
    expect(await r.json()).toEqual({ claimed: true });
  });

  it("odrzuca zły format danych", async () => {
    const r = await hello(post("/api/device/hello", { id: "LUMI-zz", secret: "x", pair: "1" }));
    expect(r.status).toBe(400);
  });
});
