// Protokół urządzenia (firmware/src/net/Net.cpp) przeciw prawdziwym trasom API na atrapie bazy
import { beforeEach, describe, expect, it, vi } from "vitest";
import { fakeDb } from "./fakeDb";

vi.mock("server-only", () => ({}));
const db = fakeDb();
vi.mock("@/lib/supabase/admin", () => ({ admin: () => db.client }));

const { POST: hello } = await import("@/app/api/device/hello/route");
const { GET: config } = await import("@/app/api/device/config/route");
const { POST: status } = await import("@/app/api/device/status/route");
const { GET: firmware } = await import("@/app/api/device/firmware/route");

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
    let r: Response = await hello(post("/api/device/hello", { id: ID, secret: SECRET, pair: PAIR, variant: "allegro", fw: "Nov 24 2026" }));
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

  it("aktualizacje: najnowsza aktywna wersja dla wariantu, nowsza niż zainstalowana", async () => {
    await hello(post("/api/device/hello", { id: ID, secret: SECRET, pair: PAIR, variant: "allegro", fw: "0.4.0", build: 4 }));
    let r = await firmware(get("/api/device/firmware"));
    expect(r.status).toBe(204);
    const rel = (build: number, variant = "allegro", active = true) => ({
      variant, build, version: `0.${build}.0`, url: `https://x/${build}.bin`, size: 1234567, sha256: "a".repeat(64), signature: "c2ln", active,
    });
    db.tables.firmware_releases.push(rel(3), rel(5), rel(7, "allegro", false), rel(9, "series"), rel(6));
    r = await firmware(get("/api/device/firmware"));
    expect(r.status).toBe(200);
    const o = await r.json();
    expect(o).toMatchObject({ build: 6, version: "0.6.0", sig: "c2ln", size: 1234567 });
    // po instalacji urządzenie zgłasza build 6 - więcej nie ma
    await status(post("/api/device/status", { version: 0, build: 6 }));
    r = await firmware(get("/api/device/firmware"));
    expect(r.status).toBe(204);
  });

  it("odrzuca zły format danych", async () => {
    const r = await hello(post("/api/device/hello", { id: "LUMI-zz", secret: "x", pair: "1" }));
    expect(r.status).toBe(400);
  });
});
