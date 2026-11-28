import { describe, expect, it, vi } from "vitest";
import { memoryDb } from "@/lib/memoryDb";
import { sample } from "./fixtures";

vi.mock("server-only", () => ({}));
const db = memoryDb();
vi.mock("@/lib/supabase/admin", () => ({ admin: () => db.client }));
vi.mock("next/cache", () => ({ revalidatePath: () => {} }));
vi.mock("next/navigation", () => ({
  redirect: (url: string) => {
    throw new Error(`REDIRECT ${url}`);
  },
}));
// Ania otworzyła stronę, gdy LUMI miało wersję 4 - zanim zapisał Marek
const stale = { id: "L1", settings: sample(), config_version: 4 };
vi.mock("@/lib/access", () => ({
  requireMember: async () => ({ user: { id: "ania" }, role: "guardian", device: stale }),
  getProfile: async () => ({ display_name: "Ania", form: "f" }),
}));

describe("zapis ustawień przez dwie osoby naraz", async () => {
  const { saveSettings } = await import("@/lib/store");
  it("zmiana Ani nakłada się na zapis Marka, zamiast go nadpisać", async () => {
    const marek = sample();
    marek.sound.ringVolume = 3;  // Marek zdążył zapisać wersję 5
    db.tables.devices.push({ id: "L1", settings: marek, config_version: 5 });

    await expect(
      saveSettings("L1", "weather", (s) => { s.weather = { place: "Gdańsk", lat: 54.35, lon: 18.65 }; }, "/d/L1/pogoda"),
    ).rejects.toThrow("REDIRECT /d/L1/pogoda?ok=1");

    const row = db.tables.devices.find((d) => d.id === "L1") as { settings: ReturnType<typeof sample>; config_version: number };
    expect(row.config_version).toBe(6);
    expect(row.settings.weather.place).toBe("Gdańsk");
    expect(row.settings.sound.ringVolume).toBe(3);
    expect(db.tables.changes.at(-1)).toMatchObject({ author: "Ania", version: 6 });
  });
});
