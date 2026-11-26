import { describe, expect, it, vi } from "vitest";
import { planAccountDeletion, type Membership } from "@/lib/account";
import { memoryDb } from "@/lib/memoryDb";

const m = (device_id: string, user_id: string, role: Membership["role"], created_at: string): Membership => ({ device_id, user_id, role, created_at });

describe("usunięcie konta", () => {
  it("opiekun z rodziną: rola przechodzi na najdawniej dodaną osobę z rodziny, nie na znajomego", () => {
    const plan = planAccountDeletion("ania", [
      m("L1", "ania", "guardian", "2026-01-01"),
      m("L1", "kasia", "friend", "2026-01-02"),
      m("L1", "marek", "family", "2026-03-01"),
      m("L1", "ola", "family", "2026-02-01"),
    ]);
    expect(plan).toEqual({ promote: [{ device_id: "L1", user_id: "ola" }], deleteDevices: [] });
  });
  it("opiekun sam: LUMI do usunięcia; tylko znajomy: on zostaje opiekunem", () => {
    const plan = planAccountDeletion("ania", [
      m("L1", "ania", "guardian", "2026-01-01"),
      m("L2", "ania", "guardian", "2026-01-01"),
      m("L2", "kasia", "friend", "2026-01-02"),
      m("L3", "ania", "family", "2026-01-01"),
    ]);
    expect(plan).toEqual({ promote: [{ device_id: "L2", user_id: "kasia" }], deleteDevices: ["L1"] });
  });
});

vi.mock("server-only", () => ({}));
const db = memoryDb();
vi.mock("@/lib/supabase/admin", () => ({ admin: () => db.client }));

describe("retencja i pliki", async () => {
  const { purgeOldData, deleteDeviceFiles } = await import("@/lib/cleanup");
  it("usuwa tylko stare dane", async () => {
    const now = Date.parse("2026-11-25T12:00:00Z");
    const day = (d: number) => new Date(now - d * 86400_000).toISOString();
    db.tables.reminder_events.push({ at: day(100) }, { at: day(10) });
    db.tables.messages.push({ created_at: day(200) }, { created_at: day(30) });
    db.tables.changes.push({ created_at: day(400) }, { created_at: day(5) });
    await purgeOldData(now);
    expect([db.tables.reminder_events.length, db.tables.messages.length, db.tables.changes.length]).toEqual([1, 1, 1]);
  });
  it("usuwa pliki urządzenia, cudzych nie rusza", async () => {
    await db.client.storage.from("photos").upload("LUMI-000001/a.565", Buffer.alloc(4));
    await db.client.storage.from("music").upload("LUMI-000001/x.mp3", Buffer.alloc(4));
    await db.client.storage.from("photos").upload("LUMI-000002/b.565", Buffer.alloc(4));
    await deleteDeviceFiles("LUMI-000001");
    expect([...db.files.keys()]).toEqual(["photos/LUMI-000002/b.565"]);
  });
});
