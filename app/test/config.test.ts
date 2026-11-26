// Kontrakt aplikacja <-> firmware: dokument z buildDeviceConfig jest zapisany w
// shared/config-contract.json i ten sam plik parsuje test firmware (test_contract w test_core).
import { describe, expect, it } from "vitest";
import fs from "node:fs";
import path from "node:path";
import { buildDeviceConfig } from "@/lib/config";
import { SettingsSchema, defaultSettings } from "@/lib/schema";
import { sample } from "./fixtures";

const CONTRACT = path.resolve(__dirname, "../../shared/config-contract.json");

describe("buildDeviceConfig", () => {
  it("zgadza się z kontraktem firmware", () => {
    const doc = buildDeviceConfig(sample(), 7, "Ania", "dodała kontakt Basia");
    const json = JSON.stringify(doc, null, 2) + "\n";
    if (process.env.UPDATE_CONTRACT) fs.writeFileSync(CONTRACT, json);
    expect(json).toBe(fs.readFileSync(CONTRACT, "utf8"));
  });

  it("usuwa spacje z numerów i pomija pustą ciszę", () => {
    const s = sample();
    s.sound.quiet = null;
    const doc = buildDeviceConfig(s, 1, "", "");
    expect(doc.contacts[0].number).toBe("+48600100200");
    expect("quiet" in doc.sound).toBe(false);
  });
});

describe("SettingsSchema", () => {
  it("przyjmuje ustawienia domyślne i przykładowe", () => {
    expect(SettingsSchema.safeParse(defaultSettings()).success).toBe(true);
    expect(SettingsSchema.safeParse(sample()).success).toBe(true);
  });
  it("odrzuca wartości, których firmware nie przyjmie", () => {
    const s = sample();
    s.display.textSize = 3;
    expect(SettingsSchema.safeParse(s).success).toBe(false);
    const t = sample();
    t.contacts[0].number = "abc";
    expect(SettingsSchema.safeParse(t).success).toBe(false);
    const u = sample();
    (u.media[0] as { freq: number }).freq = 150;
    expect(SettingsSchema.safeParse(u).success).toBe(false);
  });
});
