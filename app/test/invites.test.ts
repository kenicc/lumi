import { describe, expect, it, vi } from "vitest";
import { memoryDb } from "@/lib/memoryDb";

vi.mock("server-only", () => ({}));
vi.mock("next/navigation", () => ({ redirect: () => { throw new Error("redirect"); } }));
vi.mock("@/lib/supabase/server", () => ({ supabaseServer: async () => ({}) }));
const db = memoryDb();
vi.mock("@/lib/supabase/admin", () => ({ admin: () => db.client }));

describe("zaproszenia e-mailem", async () => {
  const { acceptInvites } = await import("@/lib/access");
  it("tylko dokładnie ten adres (bez względu na wielkość liter), bez znaków wzorca", async () => {
    db.tables.invites.push({ id: "i1", device_id: "L1", email: "kasia@example.com", role: "family" });
    await acceptInvites("obcy", "kasi_@example.com");
    await acceptInvites("obcy2", "%@example.com");
    expect(db.tables.members).toHaveLength(0);
    await acceptInvites("kasia", " Kasia@Example.com");
    expect(db.tables.members).toEqual([expect.objectContaining({ device_id: "L1", user_id: "kasia", role: "family" })]);
    expect(db.tables.invites).toHaveLength(0);
  });
});
