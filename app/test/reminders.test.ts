import { describe, expect, it, vi } from "vitest";
import { daysText, todayPlan, warsawNow } from "@/lib/reminders";
import { summarize } from "@/lib/summary";
import { currentSettings } from "@/lib/store";
import { defaultSettings } from "@/lib/schema";
import { sample } from "./fixtures";

vi.mock("server-only", () => ({}));

describe("przypomnienia", () => {
  it("dni tygodnia po polsku", () => {
    expect(daysText([])).toBe("codziennie");
    expect(daysText([1, 2, 3, 4, 5])).toBe("pn-pt");
    expect(daysText([1, 3, 5])).toBe("pn, śr, pt");
  });

  it("czas polski: wtorek 24.11.2026, 9:00", () => {
    const w = warsawNow(new Date("2026-11-24T08:00:00Z"));
    expect(w).toEqual({ date: "2026-11-24", weekday: 2, minute: 540 });
  });

  it("plan dnia: zrobione, bez informacji, później, nie dziś", () => {
    const s = sample();
    s.reminders.push({ id: "sb", text: "Sobota", time: "10:00", days: [6], confirm: true });
    s.reminders.push({ id: "wiecz", text: "Wieczór", time: "20:00", days: [], confirm: true });
    const now = new Date("2026-11-24T14:50:00Z"); // 15:50 w Polsce, wtorek
    const plan = todayPlan(s.reminders, [{ reminder_id: "leki", status: "done", at: "2026-11-24T07:03:00Z" }], now);
    expect(plan.map((p) => [p.reminder.id, p.state])).toEqual([
      ["leki", "done"],
      ["woda", "unknown"],
      ["wiecz", "upcoming"],
    ]);
  });

  it("zdanie o zmianie", () => {
    const a = sample();
    const b = sample();
    b.reminders.push({ id: "x", text: "Obiad", time: "13:00", days: [], confirm: false });
    expect(summarize(a, b, "f")).toBe("dodała przypomnienie Obiad");
  });
});

describe("currentSettings", () => {
  it("stare ustawienia bez nowej sekcji dostają wartość domyślną zamiast utraty danych", () => {
    const old = sample() as Record<string, unknown>;
    delete old.reminders;
    const s = currentSettings(old);
    expect(s.reminders).toEqual([]);
    expect(s.contacts).toHaveLength(2);
  });
  it("uszkodzone dane: błąd, nie ciche zastąpienie domyślnymi", () => {
    expect(() => currentSettings({ ...defaultSettings(), contacts: "zepsute" })).toThrow();
    expect(currentSettings(null).contacts).toEqual([]);
  });
});
