import { describe, expect, it } from "vitest";
import { plural } from "@/components/ui";

describe("plural", () => {
  it("polska odmiana", () => {
    const f = (n: number) => plural(n, "nowa propozycja", "nowe propozycje", "nowych propozycji");
    expect([1, 2, 4, 5, 12, 14, 22, 25].map(f)).toEqual([
      "nowa propozycja", "nowe propozycje", "nowe propozycje", "nowych propozycji",
      "nowych propozycji", "nowych propozycji", "nowe propozycje", "nowych propozycji",
    ]);
  });
});
