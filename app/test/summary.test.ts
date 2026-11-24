import { describe, expect, it } from "vitest";
import { pairedText, summarize } from "@/lib/summary";
import { sample } from "./fixtures";

describe("summarize", () => {
  it("dodanie kontaktu w trzech formach", () => {
    const a = sample();
    const b = sample();
    b.contacts.push({ id: "marek", name: "Marek", number: "602300400", color: "#2E7D32", autoAnswer: false });
    expect(summarize(a, b, "f")).toBe("dodała kontakt Marek");
    expect(summarize(a, b, "m")).toBe("dodał kontakt Marek");
    expect(summarize(a, b, "n")).toBe("dodaje kontakt Marek");
  });
  it("usunięcie stacji", () => {
    const a = sample();
    const b = sample();
    b.media = b.media.filter((m) => m.name !== "Jedynka");
    expect(summarize(a, b, "f")).toBe("usunęła stację Jedynka");
  });
  it("kilka zmian naraz", () => {
    const a = sample();
    const b = sample();
    b.display.textSize = 2;
    b.sound.ringVolume = 10;
    expect(summarize(a, b, "m")).toBe("zmienił wygląd ekranu i inne ustawienia");
  });
  it("zmiana kolejności kontaktów", () => {
    const a = sample();
    const b = sample();
    b.contacts.reverse();
    expect(summarize(a, b, "n")).toBe("zmienia kontakty");
  });
  it("parowanie", () => {
    expect(pairedText("f")).toBe("połączyła LUMI z aplikacją");
  });
});
