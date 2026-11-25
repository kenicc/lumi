import "server-only";
// Tryb demo (LUMI_DEMO=1): aplikacja działa bez Supabase, na bazie w pamięci z przykładowym
// LUMI. Do pokazywania i pracy nad wyglądem. Nigdy nie ustawiaj tego na prawdziwym serwerze.
import { buildDeviceConfig } from "./config";
import { memoryDb } from "./memoryDb";
import { defaultSettings } from "./schema";

export const DEMO = process.env.LUMI_DEMO === "1";
export const DEMO_USER = { id: "00000000-0000-0000-0000-00000000demo", email: "ania@example.com" };

type Db = ReturnType<typeof memoryDb>;
const g = globalThis as unknown as { lumiDemo?: Db };

function seed(): Db {
  const db = memoryDb();
  const s = defaultSettings();
  s.contacts = [
    { id: "ania", name: "Ania", number: "+48 600 100 200", color: "#3D8BFD", autoAnswer: true },
    { id: "basia", name: "Basia", number: "+48 601 200 300", color: "#C2185B", autoAnswer: false },
    { id: "marek", name: "Marek", number: "+48 602 300 400", color: "#2E7D32", autoAnswer: false },
    { id: "lekarz", name: "Przychodnia", number: "+48 12 345 67 89", color: "#6D4C41", autoAnswer: false },
  ];
  s.home.slots = ["contact:ania", "media", "weather"];
  s.media = [
    { type: "fm", name: "Jedynka", freq: 99.4 },
    { type: "fm", name: "Radio Kraków", freq: 101.6 },
    { type: "web", name: "Polskie Radio Chopin", url: "http://stream.polskieradio.pl/chopin" },
    { type: "playlist", name: "Piosenki od Ani", from: "Ania", urls: ["https://example.com/a.mp3"] },
  ];
  s.weather = { place: "Kraków", lat: 50.06, lon: 19.94 };
  const now = Date.now();
  db.tables.devices.push({
    id: "LUMI-C3D4E5", secret_hash: "0".repeat(64), pair_hash: null, name: "Mama", settings: s,
    config: buildDeviceConfig(s, 7, "Ania", "dodała kontakt Przychodnia"), config_version: 7, applied_version: 7,
    status: { battery: 64, charging: false, signal: 3, registered: true, lte: false, sim: true, operator: "Orange", modem: "SIM800 (2G)", wifiRssi: -58, uptime: 93600, boots: 12, reset: "zasilanie", errors: ["4120s [pogoda] HTTP -1"] },
    variant: "allegro", fw: "0.4.0", fw_build: 4, offline_alerted: false, last_seen: new Date(now - 40_000).toISOString(),
  });
  db.tables.profiles.push({ user_id: DEMO_USER.id, display_name: "Ania", form: "f" });
  db.tables.profiles.push({ user_id: "u-marek", display_name: "Marek", form: "m" });
  db.tables.members.push({ device_id: "LUMI-C3D4E5", user_id: DEMO_USER.id, role: "guardian" });
  db.tables.members.push({ device_id: "LUMI-C3D4E5", user_id: "u-marek", role: "family" });
  db.tables.invites.push({ id: "i1", device_id: "LUMI-C3D4E5", email: "kasia@example.com", role: "friend" });
  const ch = (v: number, author: string, summary: string, minAgo: number) =>
    db.tables.changes.push({ id: v, device_id: "LUMI-C3D4E5", author, summary, version: v, created_at: new Date(now - minAgo * 60_000).toISOString() });
  ch(5, "Marek", "dodał stację Radio Kraków", 60 * 26);
  ch(6, "Ania", "zmieniła głośność", 60 * 3);
  ch(7, "Ania", "dodała kontakt Przychodnia", 12);
  return db;
}

export function demoDb(): Db {
  return (g.lumiDemo ??= seed());
}
