import { describe, expect, it } from "vitest";
import { applySettingOp, canonicalNumber, fromLocal, sameNumber } from "@/lib/deviceSettings";
import { defaultSettings } from "@/lib/schema";
import { sample } from "./fixtures";

describe("zmiany zrobione na samym LUMI", () => {
  it("numer porównywany w całości, bez względu na zapis", () => {
    expect(canonicalNumber("+48 600 100 200")).toBe("48600100200");
    expect(sameNumber("0048600100200", "600100200")).toBe(true);
    expect(sameNumber("+491701234567", "701234567")).toBe(false);
  });

  it("dodanie i usunięcie osoby; ponowione zdarzenie nic nie zmienia", () => {
    const s = sample();
    const n = s.contacts.length;
    const added = applySettingOp(s, { op: "addContact", id: "d1", name: "Córka", number: "605 333 444" })!;
    expect(added.contacts).toHaveLength(n + 1);
    expect(added.contacts.at(-1)).toMatchObject({ id: "d1", name: "Córka", number: "605333444", autoAnswer: false });
    expect(applySettingOp(added, { op: "addContact", id: "d1", name: "Córka", number: "605333444" })).toBeNull();
    // ten sam numer zapisany inaczej to ta sama osoba
    expect(applySettingOp(added, { op: "addContact", id: "d2", name: "Ktoś", number: "+48605333444" })).toBeNull();
    const first = s.contacts[0];
    s.home.slots[0] = `contact:${first.id}`;
    const removed = applySettingOp(s, { op: "removeContact", id: first.id })!;
    expect(removed.contacts.find((c) => c.id === first.id)).toBeUndefined();
    expect(removed.home.slots[0]).toBe("");
    expect(applySettingOp(removed, { op: "removeContact", id: first.id })).toBeNull();
  });

  it("wielkość tekstu i dzwonek", () => {
    const s = sample();
    expect(applySettingOp(s, { op: "textSize", value: 2 })!.display.textSize).toBe(2);
    expect(applySettingOp(s, { op: "ringVolume", value: s.sound.ringVolume })).toBeNull();
  });

  it("parowanie zaczyna od ustawień z LUMI, bez duplikatów", () => {
    const s = fromLocal(defaultSettings(), {
      contacts: [{ id: "d1", name: "Córka", number: "605333444" }, { name: "Józef", number: "+48 601 200 300" }, { name: "Józef 2", number: "601200300" }],
      textSize: 1,
      ringVolume: 6,
    });
    expect(s.contacts.map((c) => c.name)).toEqual(["Córka", "Józef"]);
    expect(s.display.textSize).toBe(1);
    expect(s.sound.ringVolume).toBe(6);
    expect(fromLocal(defaultSettings(), null)).toEqual(defaultSettings());
  });
});
